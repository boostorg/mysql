#ifndef INCLUDE_MYSQL_PREPARED_STATEMENT_HPP_
#define INCLUDE_MYSQL_PREPARED_STATEMENT_HPP_

#include "mysql/resultset.hpp"
#include "mysql/impl/channel.hpp"
#include "mysql/impl/messages.hpp"
#include <optional>

namespace mysql
{

template <typename Stream>
class prepared_statement
{
	detail::channel<Stream>* channel_ {};
	detail::com_stmt_prepare_ok_packet stmt_msg_;
public:
	prepared_statement() = default;
	prepared_statement(detail::channel<Stream>& chan, const detail::com_stmt_prepare_ok_packet& msg) noexcept:
		channel_(&chan), stmt_msg_(msg) {}

	bool valid() const noexcept { return channel_ != nullptr; }
	unsigned id() const noexcept { assert(valid()); return stmt_msg_.statement_id.value; }
	unsigned num_params() const noexcept { assert(valid()); return stmt_msg_.num_params.value; }

	template <typename Collection>
	resultset<Stream> execute(const Collection& params, error_code& err, error_info& info) const
	{
		return execute(std::begin(params), std::end(params), err, info);
	}

	template <typename ForwardIterator>
	resultset<Stream> execute(ForwardIterator params_first, ForwardIterator params_last, error_code&, error_info&) const;

	static constexpr std::array<value, 0> no_params {};
};

}

#include "mysql/impl/prepared_statement.ipp"

#endif /* INCLUDE_MYSQL_PREPARED_STATEMENT_HPP_ */
