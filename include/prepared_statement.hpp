#ifndef INCLUDE_PREPARED_STATEMENT_HPP_
#define INCLUDE_PREPARED_STATEMENT_HPP_

#include "messages.hpp"
#include "mysql_stream.hpp"
#include <stdexcept>

namespace mysql
{

struct ParamDefinition
{
	std::vector<std::uint8_t> packet;
	ColumnDefinition value;
};

class PreparedStatement
{
	MysqlStream* stream_;
	int4 statement_id_;
	std::vector<ParamDefinition> params_;
	std::vector<ParamDefinition> columns_;

	void do_execute(const StmtExecute& message);
public:
	PreparedStatement(MysqlStream& stream, int4 statement_id,
			std::vector<ParamDefinition>&& params, std::vector<ParamDefinition>&& columns):
		stream_ {&stream}, statement_id_ {statement_id}, params_{std::move(params)},
		columns_ {std::move(columns)} {};
	PreparedStatement(const PreparedStatement&) = delete;
	PreparedStatement(PreparedStatement&&) = default;
	const PreparedStatement& operator=(const PreparedStatement&) = delete;
	PreparedStatement& operator=(PreparedStatement&&) = default;
	~PreparedStatement() = default;

	int4 statement_id() const { return statement_id_; }
	const std::vector<ParamDefinition>& params() const { return params_; }
	const std::vector<ParamDefinition>& columns() const { return columns_; }

	template <typename... Params>
	void execute(Params&&... params);
	// execute(Something&&... params): StatementResponse
	// execute() // uses already-bound params
	// close(Connection)
	// Destructor should try to auto-close

	static PreparedStatement prepare(MysqlStream& stream, std::string_view query);
};


// Implementations
namespace detail
{

std::size_t field_type_to_variant_index(FieldType value);

template <typename Param>
void set_param_value(const ParamDefinition& definition, StmtParamValue& output, Param&& param)
{
	FieldType type = definition.value.type;
	output.field_type = type;
	output.is_signed = true; // TODO: where can we take this from?
	output.value = BinaryValue(std::forward<Param>(param));
	if (output.value.index() != field_type_to_variant_index(type))
	{
		throw std::invalid_argument {"Wrong parameter type passed to prepared statement"};
	}
}

template <typename Param0, typename... Params>
void fill_execute_msg_impl(
	std::vector<ParamDefinition>::const_iterator param_def,
	std::vector<StmtParamValue>::iterator param_output,
	Param0&& param,
	Params&&... tail
)
{
	set_param_value(*param_def, *param_output, std::forward<Param0>(param));
	fill_execute_msg_impl(std::next(param_def), std::next(param_output), std::forward<Params>(tail)...);
}

template <typename Param>
void fill_execute_msg_impl(
	std::vector<ParamDefinition>::const_iterator param_def,
	std::vector<StmtParamValue>::iterator param_output,
	Param&& param
)
{
	set_param_value(*param_def, *param_output, std::forward<Param>(param));
}


template <typename... Args>
void fill_execute_msg(const std::vector<ParamDefinition>& param_defs, StmtExecute& output, Args&&... args)
{
	if (sizeof...(args) != param_defs.size())
	{
		throw std::out_of_range {"Wrong number of parameters passed to prepared statement"};
	}
	output.new_params_bind_flag = 1;
	output.param_values.resize(param_defs.size());
	fill_execute_msg_impl(param_defs.begin(), output.param_values.begin(), std::forward<Args>(args)...);
}

}

}


template <typename... Params>
void mysql::PreparedStatement::execute(Params&&... actual_params)
{
	StmtExecute message
	{
		statement_id_,
		0 // TODO: what is this parameter? cursor type??
	};
	detail::fill_execute_msg(params_, message, std::forward<Params>(actual_params)...);
	do_execute(message);
}


#endif /* INCLUDE_PREPARED_STATEMENT_HPP_ */
