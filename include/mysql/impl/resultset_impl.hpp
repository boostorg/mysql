#ifndef INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_
#define INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_

#include "mysql/impl/query.hpp"
#include <boost/asio/coroutine.hpp>
#include <boost/asio/yield.hpp>
#include <cassert>
#include <limits>

template <typename StreamType>
const mysql::row* mysql::resultset<StreamType>::fetch_one(
	error_code& err
)
{
	assert(valid());
	if (complete())
	{
		err.clear();
		return nullptr;
	}
	auto result = detail::fetch_text_row(
		*channel_,
		meta_.fields(),
		buffer_,
		current_row_.values(),
		ok_packet_,
		err
	);
	eof_received_ = result == detail::fetch_result::eof;
	return result == detail::fetch_result::row ? &current_row_ : nullptr;
}

template <typename StreamType>
const mysql::row* mysql::resultset<StreamType>::fetch_one()
{
	error_code errc;
	const row* res = fetch_one(errc);
	detail::check_error_code(errc);
	return res;
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_many(
	std::size_t count,
	error_code& err
)
{
	assert(valid());

	err.clear();
	std::vector<mysql::owning_row> res;

	if (!complete()) // support calling fetch on already exhausted resultset
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			detail::bytestring buff;
			std::vector<value> values;

			auto result = detail::fetch_text_row(
				*channel_,
				meta_.fields(),
				buff,
				values,
				ok_packet_,
				err
			);
			eof_received_ = result == detail::fetch_result::eof;
			if (result == detail::fetch_result::row)
			{
				res.emplace_back(std::move(values), std::move(buff));
			}
			else
			{
				break;
			}
		}
	}

	return res;
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_many(
	std::size_t count
)
{
	error_code errc;
	auto res = fetch_many(count, errc);
	detail::check_error_code(errc);
	return res;
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_all(
	error_code& err
)
{
	return fetch_many(std::numeric_limits<std::size_t>::max(), err);
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_all()
{
	return fetch_many(std::numeric_limits<std::size_t>::max());
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(mysql::error_code, const mysql::row*))
mysql::resultset<StreamType>::async_fetch_one(
	CompletionToken&& token
)
{
	using HandlerSignature = void(error_code, const row*);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

	struct Op: BaseType, boost::asio::coroutine
	{
		resultset<StreamType>& resultset_;

		Op(
			HandlerType&& handler,
			resultset<StreamType>& obj
		):
			BaseType(std::move(handler), obj.channel_->next_layer().get_executor()),
			resultset_(obj) {};

		void operator()(
			error_code err,
			detail::fetch_result result,
			bool cont=true
		)
		{
			reenter(*this)
			{
				if (resultset_.complete())
				{
					this->complete(cont, error_code(), nullptr);
				}
				else
				{
					yield detail::async_fetch_text_row(
						*resultset_.channel_,
						resultset_.meta_.fields(),
						resultset_.buffer_,
						resultset_.current_row_.values(),
						resultset_.ok_packet_,
						std::move(*this)
					);
					resultset_.eof_received_ = result == detail::fetch_result::eof;
					this->complete(
						cont,
						err,
						result == detail::fetch_result::row ? &resultset_.current_row_ : nullptr
					);
				}
			}
		}
	};

	assert(valid());

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	Op(
		std::move(initiator.completion_handler),
		*this
	)(error_code(), detail::fetch_result::error, false);
	return initiator.result.get();
}

#include <boost/asio/unyield.hpp>


#endif /* INCLUDE_MYSQL_IMPL_RESULTSET_IMPL_HPP_ */
