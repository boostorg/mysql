
#include "prepared_statement.hpp"
#include "message_serialization.hpp"

using namespace std;

static void read_param(mysql::MysqlStream& stream, mysql::ParamDefinition& output)
{
	stream.read(output.packet);
	mysql::deserialize(output.packet, output.value);
}

mysql::PreparedStatement mysql::PreparedStatement::prepare(MysqlStream& stream, std::string_view query)
{
	// Write the prepare request
	StmtPrepare packet {{query}};
	DynamicBuffer write_buffer;
	serialize(write_buffer, packet);
	stream.reset_sequence_number();
	stream.write(write_buffer.get());

	// Get the prepare response
	std::vector<std::uint8_t> read_buffer;
	stream.read(read_buffer);
	int1 status = get_message_type(read_buffer);
	if (status != 0)
		throw std::runtime_error {"Error preparing statement" + std::string{query}};
	StmtPrepareResponseHeader response;
	deserialize(read_buffer.data() + 1, read_buffer.data() + read_buffer.size(), response);

	// Read the parameters and columns if any
	std::vector<ParamDefinition> params (response.num_params);
	for (int2 i = 0; i < response.num_params; ++i)
		read_param(stream, params[i]);
	std::vector<ParamDefinition> columns (response.num_columns);
	for (int2 i = 0; i < response.num_columns; ++i)
		read_param(stream, columns[i]);

	return PreparedStatement {stream, response.statement_id, move(params), move(columns)};
}
