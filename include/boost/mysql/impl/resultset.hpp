#ifndef MYSQL_ASIO_IMPL_RESULTSET_HPP
#define MYSQL_ASIO_IMPL_RESULTSET_HPP

#include "boost/mysql/detail/network_algorithms/read_row.hpp"
#include <boost/asio/coroutine.hpp>
#include <cassert>
#include <limits>
#include <boost/asio/yield.hpp>

template <typename StreamType>
const mysql::row* mysql::resultset<StreamType>::fetch_one(
	error_code& err,
	error_info& info
)
{
	assert(valid());

	err.clear();
	info.clear();

	if (complete())
	{
		return nullptr;
	}
	auto result = detail::read_row(
		deserializer_,
		*channel_,
		meta_.fields(),
		buffer_,
		current_row_.values(),
		ok_packet_,
		err,
		info
	);
	eof_received_ = result == detail::read_row_result::eof;
	return result == detail::read_row_result::row ? &current_row_ : nullptr;
}

template <typename StreamType>
const mysql::row* mysql::resultset<StreamType>::fetch_one()
{
	error_code errc;
	error_info info;
	const row* res = fetch_one(errc, info);
	detail::check_error_code(errc, info);
	return res;
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_many(
	std::size_t count,
	error_code& err,
	error_info& info
)
{
	assert(valid());

	err.clear();
	info.clear();

	std::vector<mysql::owning_row> res;

	if (!complete()) // support calling fetch on already exhausted resultset
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			detail::bytestring buff;
			std::vector<value> values;

			auto result = detail::read_row(
				deserializer_,
				*channel_,
				meta_.fields(),
				buff,
				values,
				ok_packet_,
				err,
				info
			);
			eof_received_ = result == detail::read_row_result::eof;
			if (result == detail::read_row_result::row)
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
	error_info info;
	auto res = fetch_many(count, errc, info);
	detail::check_error_code(errc, info);
	return res;
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_all(
	error_code& err,
	error_info& info
)
{
	return fetch_many(std::numeric_limits<std::size_t>::max(), err, info);
}

template <typename StreamType>
std::vector<mysql::owning_row> mysql::resultset<StreamType>::fetch_all()
{
	return fetch_many(std::numeric_limits<std::size_t>::max());
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, const mysql::row*)
)
mysql::resultset<StreamType>::async_fetch_one(
	CompletionToken&& token
)
{
	using HandlerSignature = void(error_code, error_info, const row*);
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
			error_info info,
			detail::read_row_result result,
			bool cont=true
		)
		{
			reenter(*this)
			{
				if (resultset_.complete())
				{
					this->complete(cont, error_code(), error_info(), nullptr);
				}
				else
				{
					yield detail::async_read_row(
						resultset_.deserializer_,
						*resultset_.channel_,
						resultset_.meta_.fields(),
						resultset_.buffer_,
						resultset_.current_row_.values(),
						resultset_.ok_packet_,
						std::move(*this)
					);
					resultset_.eof_received_ = result == detail::read_row_result::eof;
					this->complete(
						cont,
						err,
						std::move(info),
						result == detail::read_row_result::row ? &resultset_.current_row_ : nullptr
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
	)(error_code(), error_info(), detail::read_row_result::error, false);
	return initiator.result.get();
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, std::vector<mysql::owning_row>)
)
mysql::resultset<StreamType>::async_fetch_many(
	std::size_t count,
	CompletionToken&& token
)
{
	using HandlerSignature = void(error_code, error_info, std::vector<owning_row>);
	using HandlerType = BOOST_ASIO_HANDLER_TYPE(CompletionToken, HandlerSignature);
	using BaseType = boost::beast::async_base<HandlerType, typename StreamType::executor_type>;

	struct OpImpl
	{
		resultset<StreamType>& parent_resultset;
		std::vector<owning_row> rows;
		detail::bytestring buffer;
		std::vector<value> values;
		std::size_t remaining;

		OpImpl(resultset<StreamType>& obj, std::size_t count):
			parent_resultset(obj), remaining(count) {};

		void row_received()
		{
			rows.emplace_back(std::move(values), std::move(buffer));
			values = std::vector<value>{};
			buffer = detail::bytestring{};
			--remaining;
		}
	};

	struct Op: BaseType, boost::asio::coroutine
	{
		std::shared_ptr<OpImpl> impl_;

		Op(
			HandlerType&& handler,
			std::shared_ptr<OpImpl>&& impl
		):
			BaseType(std::move(handler), impl->parent_resultset.channel_->next_layer().get_executor()),
			impl_(std::move(impl))
		{
		};

		void operator()(
			error_code err,
			error_info info,
			detail::read_row_result result,
			bool cont=true
		)
		{
			reenter(*this)
			{
				while (!impl_->parent_resultset.complete() && impl_->remaining > 0)
				{
					yield detail::async_read_row(
						impl_->parent_resultset.deserializer_,
						*impl_->parent_resultset.channel_,
						impl_->parent_resultset.meta_.fields(),
						impl_->buffer,
						impl_->values,
						impl_->parent_resultset.ok_packet_,
						std::move(*this)
					);
					if (result == detail::read_row_result::error)
					{
						this->complete(cont, err, std::move(info), std::move(impl_->rows));
						yield break;
					}
					else if (result == detail::read_row_result::eof)
					{
						impl_->parent_resultset.eof_received_ = true;
					}
					else
					{
						impl_->row_received();
					}
				}
				this->complete(cont, err, error_info(), std::move(impl_->rows));
			}
		}
	};

	assert(valid());

	boost::asio::async_completion<CompletionToken, HandlerSignature> initiator(token);

	Op(
		std::move(initiator.completion_handler),
		std::make_shared<OpImpl>(*this, count)
	)(error_code(), error_info(), detail::read_row_result::error, false);
	return initiator.result.get();
}

template <typename StreamType>
template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(
	CompletionToken,
	void(mysql::error_code, mysql::error_info, std::vector<mysql::owning_row>)
)
mysql::resultset<StreamType>::async_fetch_all(
	CompletionToken&& token
)
{
	return async_fetch_many(
		std::numeric_limits<std::size_t>::max(),
		std::forward<CompletionToken>(token)
	);
}


#include <boost/asio/unyield.hpp>


#endif
