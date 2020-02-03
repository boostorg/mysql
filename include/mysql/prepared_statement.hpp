#ifndef INCLUDE_MYSQL_PREPARED_STATEMENT_HPP_
#define INCLUDE_MYSQL_PREPARED_STATEMENT_HPP_

#include "mysql/impl/messages.hpp"
#include <optional>

namespace mysql
{

class prepared_statement
{
	std::optional<detail::com_stmt_prepare_ok_packet> stmt_msg_;
public:
	prepared_statement() = default;
	prepared_statement(const detail::com_stmt_prepare_ok_packet& msg) noexcept:
		stmt_msg_(msg) {}

	bool valid() const noexcept { return static_cast<bool>(stmt_msg_); }
	unsigned id() const noexcept { assert(valid()); return stmt_msg_->statement_id.value; }
	unsigned num_params() const noexcept { assert(valid()); return stmt_msg_->num_params.value; }
};

}



#endif /* INCLUDE_MYSQL_PREPARED_STATEMENT_HPP_ */
