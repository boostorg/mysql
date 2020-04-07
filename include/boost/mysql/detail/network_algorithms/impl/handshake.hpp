#ifndef MYSQL_ASIO_IMPL_HANDSHAKE_IPP
#define MYSQL_ASIO_IMPL_HANDSHAKE_IPP

#include "boost/mysql/detail/network_algorithms/common.hpp"
#include "boost/mysql/detail/protocol/capabilities.hpp"
#include "boost/mysql/detail/protocol/handshake_messages.hpp"
#include "boost/mysql/detail/auth/auth_calculator.hpp"
#include <boost/asio/yield.hpp>

namespace boost {
namespace mysql {
namespace detail {

inline std::uint8_t get_collation_first_byte(collation value)
{
	return static_cast<std::uint16_t>(value) % 0xff;
}

inline capabilities conditional_capability(bool condition, std::uint32_t cap)
{
	return capabilities(condition ? cap : 0);
}

inline error_code deserialize_handshake(
	boost::asio::const_buffer buffer,
	handshake_packet& output,
	error_info& info
)
{
	deserialization_context ctx (boost::asio::buffer(buffer), capabilities());
	auto [err, msg_type] = deserialize_message_type(ctx);
	if (err) return err;
	if (msg_type == handshake_protocol_version_9)
	{
		return make_error_code(errc::server_unsupported);
	}
	else if (msg_type == error_packet_header)
	{
		return process_error_packet(ctx, info);
	}
	else if (msg_type != handshake_protocol_version_10)
	{
		return make_error_code(errc::protocol_value_error);
	}
	return deserialize_message(output, ctx);
}

// When receiving an auth response from the server, several things can happen:
//  - An OK packet. It means we are done with the auth phase. auth_result::complete.
//  - An auth switch response. It means we should change the auth plugin,
//    recalculate the auth response and send it back. auth_result::send_more_data.
//  - An auth more data. Same as auth switch response, but without changing
//    the authentication plugin. Also auth_result::send_more_data.
//  - An auth more data with a challenge equals to fast_auth_complete_challenge.
//    This means auth is complete and we should wait for an OK packet (auth_result::wait_for_ok).
//    I have no clue why the server sends this instead of just an OK packet. It
//    happens just for caching_sha2_password.
enum class auth_result
{
	complete,
	send_more_data,
	wait_for_ok,
	invalid
};

class handshake_processor
{
	connection_params params_;
	capabilities negotiated_caps_;
	auth_calculator auth_calc_;
public:
	handshake_processor(const connection_params& params): params_(params) {};
	capabilities negotiated_capabilities() const noexcept { return negotiated_caps_; }
	const connection_params& params() const noexcept { return params_; }
	bool use_ssl() const noexcept { return negotiated_caps_.has(CLIENT_SSL); }

	// Initial greeting processing
	error_code process_capabilities(const handshake_packet& handshake)
	{
		auto ssl = params_.ssl().mode();
		capabilities server_caps (handshake.capability_falgs.value);
		capabilities required_caps = mandatory_capabilities |
				conditional_capability(!params_.database().empty(), CLIENT_CONNECT_WITH_DB) |
				conditional_capability(ssl == ssl_mode::require, CLIENT_SSL);
		if (!server_caps.has_all(required_caps))
		{
			return make_error_code(errc::server_unsupported);
		}
		negotiated_caps_ = server_caps & (required_caps | optional_capabilities |
				conditional_capability(ssl == ssl_mode::enable, CLIENT_SSL));
		return error_code();
	}

	error_code process_handshake(bytestring& buffer, error_info& info)
	{
		// Deserialize server greeting
		handshake_packet handshake;
		auto err = deserialize_handshake(boost::asio::buffer(buffer), handshake, info);
		if (err) return err;

		// Check capabilities
		err = process_capabilities(handshake);
		if (err) return err;

		// Compute auth response
		return auth_calc_.calculate(
			handshake.auth_plugin_name.value,
			params_.password(),
			handshake.auth_plugin_data.value,
			use_ssl()
		);
	}

	// Response to that initial greeting
	void compose_ssl_request(bytestring& buffer)
	{
		ssl_request sslreq {
			int4(negotiated_capabilities().get()),
			int4(static_cast<std::uint32_t>(MAX_PACKET_SIZE)),
			int1(get_collation_first_byte(params_.connection_collation())),
			{}
		};

		// Serialize and send
		serialize_message(sslreq, negotiated_caps_, buffer);
	}

	void compose_handshake_response(bytestring& buffer)
	{
		// Compose response
		handshake_response_packet response {
			int4(negotiated_caps_.get()),
			int4(static_cast<std::uint32_t>(MAX_PACKET_SIZE)),
			int1(get_collation_first_byte(params_.connection_collation())),
			string_null(params_.username()),
			string_lenenc(auth_calc_.response()),
			string_null(params_.database()),
			string_null(auth_calc_.plugin_name())
		};

		// Serialize
		serialize_message(response, negotiated_caps_, buffer);
	}

	// Server handshake response
	error_code process_handshake_server_response(
		bytestring& buffer,
		auth_result& result,
		error_info& info
	)
	{
		deserialization_context ctx (boost::asio::buffer(buffer), negotiated_caps_);
		auto [err, msg_type] = deserialize_message_type(ctx);
		if (err) return err;
		if (msg_type == ok_packet_header)
		{
			// Auth success via fast auth path
			result = auth_result::complete;
			return error_code();
		}
		else if (msg_type == error_packet_header)
		{
			return process_error_packet(ctx, info);
		}
		else if (msg_type == auth_switch_request_header)
		{
			// We have received an auth switch request. Deserialize it
			auth_switch_request_packet auth_sw;
			err = deserialize_message(auth_sw, ctx);
			if (err) return err;

			// Compute response
			auth_switch_response_packet auth_sw_res;
			err = auth_calc_.calculate(
				auth_sw.plugin_name.value,
				params_.password(),
				auth_sw.auth_plugin_data.value,
				use_ssl()
			);
			if (err) return err;
			auth_sw_res.auth_plugin_data.value = auth_calc_.response();

			// Serialize
			serialize_message(auth_sw_res, negotiated_caps_, buffer);

			result = auth_result::send_more_data;
			return error_code();
		}
		else if (msg_type == auth_more_data_header)
		{
			// We have received an auth more data request. Deserialize it
			auth_more_data_packet more_data;
			err = deserialize_message(more_data, ctx);
			if (err) return err;

			std::string_view challenge = more_data.auth_plugin_data.value;
			if (challenge == fast_auth_complete_challenge)
			{
				result = auth_result::wait_for_ok;
				return error_code();
			}

			// Compute response
			err = auth_calc_.calculate(
				auth_calc_.plugin_name(),
				params_.password(),
				challenge,
				use_ssl()
			);
			if (err) return err;

			serialize_message(
				auth_switch_response_packet {string_eof(auth_calc_.response())},
				negotiated_caps_,
				buffer
			);

			result = auth_result::send_more_data;
			return error_code();
		}
		else
		{
			return make_error_code(errc::protocol_value_error);
		}
	}
};

} // detail
} // mysql
} // boost

template <typename StreamType>
void boost::mysql::detail::hanshake(
	channel<StreamType>& channel,
	const connection_params& params,
	error_code& err,
	error_info& info
)
{
	// Set up processor
	handshake_processor processor (params);

	// Read server greeting
	channel.read(channel.shared_buffer(), err);
	if (err) return;

	// Process server greeting (handshake)
	err = processor.process_handshake(channel.shared_buffer(), info);
	if (err) return;

	// Setup SSL if required
	if (processor.use_ssl())
	{
		// Send SSL request
		processor.compose_ssl_request(channel.shared_buffer());
		channel.write(boost::asio::buffer(channel.shared_buffer()), err);
		if (err) return;

		// SSL handshake
		channel.ssl_handshake(err);
		if (err) return;
	}

	// Handshake response
	processor.compose_handshake_response(channel.shared_buffer());
	channel.write(boost::asio::buffer(channel.shared_buffer()), err);
	if (err) return;

	auth_result auth_outcome = auth_result::invalid;
	while (auth_outcome != auth_result::complete)
	{
		// Receive response
		channel.read(channel.shared_buffer(), err);
		if (err) return;

		// Process it
		err = processor.process_handshake_server_response(channel.shared_buffer(), auth_outcome, info);
		if (err) return;

		if (auth_outcome == auth_result::send_more_data)
		{
			// We received an auth switch request and we have the response ready to be sent
			channel.write(boost::asio::buffer(channel.shared_buffer()), err);
			if (err) return;
		}
	};

	channel.set_current_capabilities(processor.negotiated_capabilities());
}

template <typename StreamType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	boost::mysql::detail::handshake_signature
)
boost::mysql::detail::async_handshake(
	channel<StreamType>& chan,
	const connection_params& params,
	CompletionToken&& token,
	error_info* info
)
{
	using HandlerSignature = handshake_signature;
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		channel<StreamType>& channel_;
		handshake_processor processor_;
		error_info info_;
		error_info* output_info_;
		auth_result auth_state_ {auth_result::invalid};

		Op(
			HandlerType&& handler,
			channel<StreamType>& channel,
			const connection_params& params,
			error_info* output_info
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			channel_(channel),
			processor_(params),
			output_info_(output_info)
		{
		}

		void complete(bool cont, error_code code)
		{
			channel_.set_current_capabilities(processor_.negotiated_capabilities());
			conditional_assign(output_info_, std::move(info_));
			BaseType::complete(cont, code);
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			// Error checking
			if (err)
			{
				complete(cont, err);
				return;
			}

			// Non-error path
			reenter(*this)
			{
				// Read server greeting
				yield channel_.async_read(channel_.shared_buffer(), std::move(*this));

				// Process server greeting
				err = processor_.process_handshake(channel_.shared_buffer(), info_);
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// SSL
				if (processor_.use_ssl())
				{
					// Send SSL request
					processor_.compose_ssl_request(channel_.shared_buffer());
					yield channel_.async_write(
						boost::asio::buffer(channel_.shared_buffer()),
						std::move(*this)
					);

					// SSL handshake
					yield channel_.async_ssl_handshake(std::move(*this));
				}

				// Compose and send handshake response
				processor_.compose_handshake_response(channel_.shared_buffer());
				yield channel_.async_write(boost::asio::buffer(channel_.shared_buffer()), std::move(*this));

				while (auth_state_ != auth_result::complete)
				{
					// Receive response
					yield channel_.async_read(channel_.shared_buffer(), std::move(*this));

					// Process it
					err = processor_.process_handshake_server_response(
						channel_.shared_buffer(),
						auth_state_,
						info_
					);
					if (err)
					{
						complete(cont, err);
						yield break;
					}

					if (auth_state_ == auth_result::send_more_data)
					{
						// We received an auth switch response and we have the response ready to be sent
						yield channel_.async_write(
							boost::asio::buffer(channel_.shared_buffer()),
							std::move(*this)
						);
					}
				}

				complete(cont, error_code());
			}
		}
	};

	Op(
		std::move(initiator.completion_handler),
		chan,
		params,
		info
	)(error_code(), false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>


#endif
