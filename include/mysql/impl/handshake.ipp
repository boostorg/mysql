#ifndef MYSQL_ASIO_IMPL_HANDSHAKE_IPP
#define MYSQL_ASIO_IMPL_HANDSHAKE_IPP

#include "mysql/impl/capabilities.hpp"
#include "mysql/impl/auth.hpp"
#include "mysql/error.hpp"
#include <boost/asio/coroutine.hpp>
#include <boost/asio/yield.hpp>
#include <boost/beast/core/async_base.hpp>
#include "mysql/impl/serialization.hpp"

namespace mysql
{
namespace detail
{

inline std::uint8_t get_collation_first_byte(collation value)
{
	return static_cast<std::uint16_t>(value) % 0xff;
}

inline error_code deserialize_handshake(
	boost::asio::const_buffer buffer,
	handshake_packet& output
)
{
	std::uint8_t msg_type;
	DeserializationContext ctx (boost::asio::buffer(buffer), capabilities());
	auto err = deserialize_message_type(msg_type, ctx);
	if (err) return err;
	if (msg_type == handshake_protocol_version_9)
	{
		return make_error_code(Error::server_unsupported);
	}
	else if (msg_type == error_packet_header)
	{
		return process_error_packet(ctx);
	}
	else if (msg_type != handshake_protocol_version_10)
	{
		return make_error_code(Error::protocol_value_error);
	}
	return deserialize_message(output, ctx);
}

class auth_response_calculator
{
	std::array<std::uint8_t, mysql_native_password::response_length> auth_response_buffer_ {};
public:
	error_code calculate(std::string_view password, std::string_view challenge, std::string_view& output)
	{
		// Blank password: we should just return an empty auth string
		if (password.empty())
		{
			output = std::string_view();
			return error_code();
		}

		// Check challenge size
		if (challenge.size() != mysql_native_password::challenge_length)
		{
			return make_error_code(Error::protocol_value_error);
		}

		// Do the calculation
		mysql_native_password::compute_auth_string(
			password,
			challenge.data(),
			auth_response_buffer_.data()
		);
		output = std::string_view(
			reinterpret_cast<const char*>(auth_response_buffer_.data()),
			auth_response_buffer_.size()
		);
		return error_code();
	}
};

class handshake_processor
{
	handshake_params params_;
	capabilities negotiated_caps_;
public:
	handshake_processor(const handshake_params& params): params_(params) {};
	capabilities negotiated_capabilities() const { return negotiated_caps_; }

	error_code process_capabilities(const handshake_packet& handshake)
	{
		capabilities server_caps (handshake.capability_falgs.value);
		capabilities required_caps =
				params_.database.empty() ?
				mandatory_capabilities :
				mandatory_capabilities | capabilities(CLIENT_CONNECT_WITH_DB);
		if (!server_caps.has_all(required_caps))
		{
			return make_error_code(Error::server_unsupported);
		}
		negotiated_caps_ = server_caps & (required_caps | optional_capabilities);
		return error_code();
	}
	void compose_handshake_response(
		std::string_view auth_response,
		handshake_response_packet& output
	)
	{
		output.client_flag.value = negotiated_caps_.get();
		output.max_packet_size.value = MAX_PACKET_SIZE;
		output.character_set.value = get_collation_first_byte(params_.connection_collation);
		output.username.value = params_.username;
		output.auth_response.value = auth_response;
		output.database.value = params_.database;
		output.client_plugin_name.value = mysql_native_password::plugin_name;
	}
	error_code compute_auth_switch_response(
		const auth_switch_request_packet& request,
		auth_switch_response_packet& output,
		auth_response_calculator& calc
	)
	{
		if (request.plugin_name.value != mysql_native_password::plugin_name)
		{
			return make_error_code(Error::unknown_auth_plugin);
		}
		return calc.calculate(
			params_.password,
			request.auth_plugin_data.value,
			output.auth_plugin_data.value
		);
	}

	error_code process_handshake(bytestring& buffer)
	{
		// Deserialize server greeting
		handshake_packet handshake;
		auto err = deserialize_handshake(boost::asio::buffer(buffer), handshake);
		if (err) return err;

		// Check capabilities
		err = process_capabilities(handshake);
		if (err) return err;

		// Authentication
		auth_response_calculator calc;
		std::string_view auth_response;
		if (handshake.auth_plugin_name.value == mysql_native_password::plugin_name)
		{
			err = calc.calculate(params_.password, handshake.auth_plugin_data.value, auth_response);
			if (err) return err;
		}

		// Compose response
		handshake_response_packet response;
		compose_handshake_response(auth_response, response);

		// Serialize
		serialize_message(response, negotiated_capabilities(), buffer);

		return error_code();
	}

	error_code process_handshake_server_response(
		bytestring& buffer,
		bool& auth_complete
	)
	{
		std::uint8_t msg_type;
		DeserializationContext ctx (boost::asio::buffer(buffer), negotiated_caps_);
		auto err = deserialize_message_type(msg_type, ctx);
		if (err) return err;
		if (msg_type == ok_packet_header)
		{
			// Auth success via fast auth path
			// TODO: is this OK_Packet useful for anything?
			auth_complete = true;
			return error_code();
		}
		else if (msg_type == error_packet_header)
		{
			return process_error_packet(ctx);
		}
		else if (msg_type != auth_switch_request_header)
		{
			return make_error_code(Error::protocol_value_error);
		}

		// We have received an auth switch request. Deserialize it
		auth_switch_request_packet auth_sw;
		err = deserialize_message(auth_sw, ctx);
		if (err) return err;

		// Compute response
		auth_switch_response_packet auth_sw_res;
		auth_response_calculator calc;
		err = compute_auth_switch_response(auth_sw, auth_sw_res, calc);
		if (err) return err;

		// Serialize
		serialize_message(auth_sw_res, negotiated_caps_, buffer);

		auth_complete = false;
		return error_code();
	}

	error_code process_auth_switch_response(
		boost::asio::const_buffer buffer
	)
	{
		DeserializationContext ctx (boost::asio::buffer(buffer), negotiated_caps_);
		std::uint8_t msg_type;
		auto err = deserialize_message_type(msg_type, ctx);
		if (err) return err;
		if (msg_type == error_packet_header)
		{
			return process_error_packet(ctx);
		}
		else if (msg_type != ok_packet_header)
		{
			return make_error_code(Error::protocol_value_error);
		}
		return error_code();
	}

};

} // detail
} // mysql


template <typename ChannelType>
void mysql::detail::hanshake(
	ChannelType& channel,
	const handshake_params& params,
	bytestring& buffer,
	error_code& err
)
{
	// Set up processor
	handshake_processor processor (params);

	// Read server greeting
	channel.read(buffer, err);
	if (err) return;

	// Process server greeting
	err = processor.process_handshake(buffer);
	if (err) return;

	// Send
	channel.write(boost::asio::buffer(buffer), err);
	if (err) return;

	// Receive response
	channel.read(buffer, err);
	if (err) return;

	// Process it
	bool auth_complete = false;
	err = processor.process_handshake_server_response(buffer, auth_complete);
	if (err) return;
	if (auth_complete)
	{
		err.clear();
		return;
	}

	// We received an auth switch response and we have the response ready to be sent
	channel.write(boost::asio::buffer(buffer), err);
	if (err) return;

	// Receive response
	channel.read(buffer, err);
	if (err) return;

	// Process it
	err = processor.process_auth_switch_response(boost::asio::buffer(buffer));
	if (err) return;

	channel.set_current_capabilities(processor.negotiated_capabilities());
}

template <typename ChannelType, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code))
mysql::detail::async_handshake(
	ChannelType& channel,
	const handshake_params& params,
	bytestring& buffer,
	CompletionToken&& token
)
{
	using HandlerSignature = void(mysql::error_code);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using StreamType = typename ChannelType::stream_type;
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		ChannelType& channel_;
		bytestring& buffer_;
		handshake_processor processor_;

		Op(
			HandlerType&& handler,
			ChannelType& channel,
			bytestring& buffer,
			const handshake_params& params
		):
			BaseType(std::move(handler), channel.next_layer().get_executor()),
			channel_(channel),
			buffer_(buffer),
			processor_(params)
		{
		}

		void complete(bool cont, error_code errc)
		{
			channel_.set_current_capabilities(processor_.negotiated_capabilities());
			BaseType::complete(cont, errc);
		}

		void operator()(
			error_code err,
			bool cont=true
		)
		{
			bool auth_complete = false;
			reenter(*this)
			{
				// Read server greeting
				yield channel_.async_read(buffer_, std::move(*this));
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// Process server greeting
				err = processor_.process_handshake(buffer_);
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// Send
				yield channel_.async_write(boost::asio::buffer(buffer_), std::move(*this));
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// Receive response
				yield channel_.async_read(buffer_, std::move(*this));
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// Process it
				err = processor_.process_handshake_server_response(buffer_, auth_complete);
				if (auth_complete) err.clear();
				if (err || auth_complete)
				{
					complete(cont, err);
					yield break;
				}

				// We received an auth switch response and we have the response ready to be sent
				yield channel_.async_write(boost::asio::buffer(buffer_), std::move(*this));
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// Receive response
				yield channel_.async_read(buffer_, std::move(*this));
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				// Process it
				err = processor_.process_auth_switch_response(boost::asio::buffer(buffer_));
				if (err)
				{
					complete(cont, err);
					yield break;
				}

				complete(cont, error_code());
			}
		}
	};

	Op(
		std::move(initiator.completion_handler),
		channel,
		buffer,
		params
	)(error_code(), false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>


#endif
