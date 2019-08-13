#ifndef INCLUDE_PREPARED_STATEMENT_HPP_
#define INCLUDE_PREPARED_STATEMENT_HPP_

#include "messages.hpp"
#include "mysql_stream.hpp"
#include <stdexcept>
#include <vector>

namespace mysql
{

struct ParamDefinition
{
	std::vector<std::uint8_t> packet;
	ColumnDefinition value;
};

class BinaryResultsetRow
{
	const std::vector<ParamDefinition>& fields_;
	std::vector<std::uint8_t> packet_;
	std::vector<BinaryValue> values_;
public:
	BinaryResultsetRow(const std::vector<ParamDefinition>& fields, std::vector<std::uint8_t>&& packet);
	const std::vector<ParamDefinition>& fields() const { return fields_; }
	const std::vector<BinaryValue>& values() const { return values_; }
};

class PreparedStatement
{
	MysqlStream* stream_;
	int4 statement_id_;
	std::vector<ParamDefinition> params_;
	std::vector<ParamDefinition> columns_;

	std::vector<BinaryResultsetRow> do_execute(const StmtExecute& message);
public:
	PreparedStatement(MysqlStream& stream, int4 statement_id,
			std::vector<ParamDefinition>&& params, std::vector<ParamDefinition>&& columns);
	PreparedStatement(const PreparedStatement&) = delete;
	PreparedStatement(PreparedStatement&&) = default;
	const PreparedStatement& operator=(const PreparedStatement&) = delete;
	PreparedStatement& operator=(PreparedStatement&&) = default;
	~PreparedStatement() = default;

	MysqlStream& next_layer() const { return *stream_; }
	int4 statement_id() const { return statement_id_; }
	const std::vector<ParamDefinition>& params() const { return params_; }
	const std::vector<ParamDefinition>& columns() const { return columns_; }

	template <typename... Params>
	std::vector<BinaryResultsetRow> execute(Params&&... params);
	// execute(Something&&... params): StatementResponse
	// execute() // uses already-bound params
	// close(Connection)
	// Destructor should try to auto-close

	static PreparedStatement prepare(MysqlStream& stream, std::string_view query);
};


// Implementations
namespace detail
{

inline void fill_execute_msg_impl(std::vector<BinaryValue>::iterator) {}

template <typename Param0, typename... Params>
void fill_execute_msg_impl(
	std::vector<BinaryValue>::iterator param_output,
	Param0&& param,
	Params&&... tail
)
{
	*param_output = std::forward<Param0>(param);
	fill_execute_msg_impl(std::next(param_output), std::forward<Params>(tail)...);
}




template <typename... Args>
void fill_execute_msg(StmtExecute& output, std::size_t num_params, Args&&... args)
{
	if (sizeof...(args) != num_params)
	{
		throw std::out_of_range {"Wrong number of parameters passed to prepared statement"};
	}
	output.new_params_bind_flag = 1;
	output.param_values.resize(num_params);
	fill_execute_msg_impl(output.param_values.begin(), std::forward<Args>(args)...);
}

}

}


template <typename... Params>
std::vector<mysql::BinaryResultsetRow> mysql::PreparedStatement::execute(Params&&... actual_params)
{
	StmtExecute message
	{
		statement_id_,
		0 // Cursor type: no cursor. TODO: allow execution with different cursor types
	};
	detail::fill_execute_msg(message, params_.size(), std::forward<Params>(actual_params)...);
	return do_execute(message);
}


#endif /* INCLUDE_PREPARED_STATEMENT_HPP_ */
