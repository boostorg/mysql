#ifndef INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_

#include "mysql/impl/basic_serialization.hpp"
#include "mysql/impl/capabilities.hpp"
#include "mysql/impl/auth.hpp"
#include "mysql/error.hpp"
//#include <boost/asio/yield.hpp>
#include <boost/beast/core/async_base.hpp>

namespace mysql
{
namespace detail
{

inline error_code deserialize_handshake(
	boost::asio::const_buffer buffer,
	msgs::handshake& output
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
		if (challenge.size() != mysql_native_password::challenge_length)
		{
			return make_error_code(Error::protocol_value_error);
		}
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
	const handshake_params& params_;
	capabilities negotiated_caps_;
public:
	handshake_processor(const handshake_params& params): params_(params) {};
	capabilities negotiated_capabilities() const { return negotiated_caps_; }

	error_code process_capabilities(const msgs::handshake& handshake)
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
		msgs::handshake_response& output
	)
	{
		output.client_flag.value = negotiated_caps_.get();
		output.max_packet_size.value = MAX_PACKET_SIZE;
		output.character_set = params_.character_set;
		output.username.value = params_.username;
		output.auth_response.value = auth_response;
		output.database.value = params_.database;
		output.client_plugin_name.value = mysql_native_password::plugin_name;
	}
	error_code compute_auth_switch_response(
		const msgs::auth_switch_request& request,
		msgs::auth_switch_response& output,
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

	template <typename Allocator>
	error_code process_handshake(bytestring<Allocator>& buffer)
	{
		// Deserialize server greeting
		msgs::handshake handshake;
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
		msgs::handshake_response response;
		compose_handshake_response(auth_response, response);

		// Serialize
		serialize_message(response, negotiated_capabilities(), buffer);

		return error_code();
	}

	template <typename Allocator>
	error_code process_handshake_server_response(
		bytestring<Allocator>& buffer,
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
		msgs::auth_switch_request auth_sw;
		err = deserialize_message(auth_sw, ctx);
		if (err) return err;

		// Compute response
		msgs::auth_switch_response auth_sw_res;
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


// TODO: support compression, SSL, more authentication methods
template <typename ChannelType, typename Allocator>
void mysql::detail::hanshake(
	ChannelType& channel,
	const handshake_params& params,
	bytestring<Allocator>& buffer,
	capabilities& output_capabilities,
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

	output_capabilities = processor.negotiated_capabilities();
}

template <typename ChannelType, typename Allocator, typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code, mysql::detail::capabilities))
mysql::detail::async_handshake(
	ChannelType& channel,
	const handshake_params& params,
	bytestring<Allocator>& buffer,
	CompletionToken&& token
)
{
	using HandlerSignature = void(mysql::error_code, mysql::detail::capabilities);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using StreamType = typename ChannelType::stream_type;
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	struct Op: BaseType, boost::asio::coroutine
	{
		ChannelType& channel_;
		bytestring<Allocator> buffer_;
		handshake_processor processor_;

		Op(
			HandlerType&& handler,
			ChannelType& channel,
			bytestring<Allocator>& buffer,
			const handshake_params& params
		):
			BaseType(std::move(handler), channel.next_later().get_executor()),
			channel_(channel),
			buffer_(buffer),
			processor_(params)
		{
		}

		void operator()(
			error_code errc,
			capabilities caps,
			bool cont=true
		)
		{
			/*reenter(*this)
			{
				this->complete(cont, error_code(), caps);
			}*/
		}
	};

	Op(std::move(initiator.completion_handler), channel, buffer, params)(
			error_code(), capabilities(), false);
	return initiator.result.get();
}



#endif /* INCLUDE_MYSQL_IMPL_HANDSHAKE_IMPL_HPP_ */
