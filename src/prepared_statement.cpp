
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

void mysql::PreparedStatement::do_execute(const StmtExecute& message)
{
	DynamicBuffer write_buffer;
	serialize(write_buffer, message);
	stream_->reset_sequence_number();
	stream_->write(write_buffer.get());
	std::vector<std::uint8_t> read_buffer;
	stream_->read(read_buffer);
	// TODO: do sth with response
}

std::size_t mysql::detail::field_type_to_variant_index(FieldType value)
{
	switch (value)
	{
	case FieldType::STRING:
	case FieldType::VARCHAR:
	case FieldType::VAR_STRING:
    case FieldType::ENUM:
    case FieldType::SET:
    case FieldType::LONG_BLOB:
    case FieldType::MEDIUM_BLOB:
    case FieldType::BLOB:
    case FieldType::TINY_BLOB:
    case FieldType::GEOMETRY:
    case FieldType::BIT:
    case FieldType::DECIMAL:
    case FieldType::NEWDECIMAL:
		return 0; // TODO: this is not very good
    case FieldType::LONGLONG:
    	return 1;
    case FieldType::LONG:
    case FieldType::INT24:
    	return 2;
    case FieldType::SHORT:
    case FieldType::YEAR:
    	return 3;
    case FieldType::TINY:
    	return 4;
    case FieldType::NULL_:
    	return 5;
    default: throw std::logic_error {"Not implemented"};
	}
}
