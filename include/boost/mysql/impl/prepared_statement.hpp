#ifndef INCLUDE_BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP_
#define INCLUDE_BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP_

#include "boost/mysql/detail/network_algorithms/execute_statement.hpp"
#include "boost/mysql/detail/network_algorithms/close_statement.hpp"
#include "boost/mysql/detail/aux/stringize.hpp"
#include <boost/beast/core/bind_handler.hpp>

template <typename Stream>
template <typename ForwardIterator>
void boost::mysql::prepared_statement<Stream>::check_num_params(
	ForwardIterator first,
	ForwardIterator last,
	error_code& err,
	error_info& info
) const
{
	auto param_count = std::distance(first, last);
	if (param_count != num_params())
	{
		err = detail::make_error_code(Error::wrong_num_params);
		info.set_message(detail::stringize(
				"prepared_statement::execute: expected ", num_params(), " params, but got ", param_count));
	}
}


template <typename Stream>
template <typename ForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
	ForwardIterator params_first,
	ForwardIterator params_last,
	error_code& err,
	error_info& info
) const
{
	assert(valid());

	mysql::resultset<Stream> res;
	err.clear();
	info.clear();

	// Verify we got passed the right number of params
	check_num_params(params_first, params_last, err, info);
	if (!err)
	{
		detail::execute_statement(
			*channel_,
			stmt_msg_.statement_id.value,
			params_first,
			params_last,
			res,
			err,
			info
		);
	}

	return res;
}

template <typename Stream>
template <typename ForwardIterator>
boost::mysql::resultset<Stream> boost::mysql::prepared_statement<Stream>::execute(
	ForwardIterator params_first,
	ForwardIterator params_last
) const
{
	error_code err;
	error_info info;
	auto res = execute(params_first, params_last, err, info);
	detail::check_error_code(err, info);
	return res;
}

template <typename StreamType>
template <typename ForwardIterator, typename CompletionToken>
auto boost::mysql::prepared_statement<StreamType>::async_execute(
	ForwardIterator params_first,
	ForwardIterator params_last,
	CompletionToken&& token
) const
{
	// Check we got passed the right number of params
	error_code err;
	error_info info;
	check_num_params(params_first, params_last, err, info);
	if (err)
	{
		using HandlerSignature = void(error_code, error_info, resultset<StreamType>);
		boost::asio::async_completion<CompletionToken, HandlerSignature> completion (token);
		return boost::asio::post(
			channel_->next_layer().get_executor(),
			boost::beast::bind_front_handler(
				std::move(completion.completion_handler),
				err,
				std::move(info),
				resultset<StreamType>()
			)
		);
	}

	// Actually execute the statement
	return detail::async_execute_statement(
		*channel_,
		stmt_msg_.statement_id.value,
		params_first,
		params_last,
		std::forward<CompletionToken>(token)
	);
}

template <typename StreamType>
void boost::mysql::prepared_statement<StreamType>::close(
	error_code& errc,
	error_info& info
)
{
	assert(valid());
	errc.clear();
	info.clear();
	detail::close_statement(*channel_, id(), errc, info);
}

template <typename StreamType>
void boost::mysql::prepared_statement<StreamType>::close()
{
	assert(valid());
	error_code errc;
	error_info info;
	detail::close_statement(*channel_, id(), errc, info);
	detail::check_error_code(errc, info);
}

template <typename StreamType>
template <typename CompletionToken>
auto boost::mysql::prepared_statement<StreamType>::async_close(
	CompletionToken&& token
)
{
	assert(valid());
	return detail::async_close_statement(*channel_, id(), std::forward<CompletionToken>(token));
}

#endif /* INCLUDE_BOOST_MYSQL_IMPL_PREPARED_STATEMENT_HPP_ */
