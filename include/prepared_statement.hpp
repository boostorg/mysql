#ifndef INCLUDE_PREPARED_STATEMENT_HPP_
#define INCLUDE_PREPARED_STATEMENT_HPP_

#include "messages.hpp"
#include "mysql_stream.hpp"

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
	// execute(Something&&... params): StatementResponse
	// execute() // uses already-bound params
	// close(Connection)
	// Destructor should try to auto-close

	static PreparedStatement prepare(MysqlStream& stream, std::string_view query);
};

}



#endif /* INCLUDE_PREPARED_STATEMENT_HPP_ */
