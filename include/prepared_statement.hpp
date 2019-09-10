#ifndef INCLUDE_PREPARED_STATEMENT_HPP_
#define INCLUDE_PREPARED_STATEMENT_HPP_

#include "messages.hpp"
#include "mysql_stream.hpp"
#include <stdexcept>
#include <vector>
#include <limits>

namespace mysql
{

struct ParamDefinition
{
	std::vector<std::uint8_t> packet;
	ColumnDefinition value;
	// TODO: copies should be disallowed
};

template <typename AsyncStream>
class BinaryResultset
{
	enum class State { initial, data_available, exhausted };

	MysqlStream<AsyncStream>* stream_;
	int4 statement_id_;
	int4 fetch_count_;
	std::vector<ParamDefinition> fields_;
	std::vector<std::uint8_t> current_packet_;
	std::vector<BinaryValue> current_values_;
	OkPacket ok_packet_;
	State state_;

	void read_metadata();
	void process_ok();
	void send_fetch();
	bool cursor_exists() const { return ok_packet_.status_flags & SERVER_STATUS_CURSOR_EXISTS; }
public:
	BinaryResultset(MysqlStream<AsyncStream>& stream, int4 stmt_id, int4 fetch_count):
		stream_ {&stream}, statement_id_ {stmt_id}, fetch_count_ {fetch_count}, ok_packet_ {},
		state_ {State::initial} { read_metadata(); };
	BinaryResultset(const BinaryResultset&) = delete;
	BinaryResultset(BinaryResultset&&) = default;
	BinaryResultset& operator=(const BinaryResultset&) = delete;
	BinaryResultset& operator=(BinaryResultset&&) = default;
	bool retrieve_next();
	const std::vector<ParamDefinition>& fields() const { return fields_; }
	const std::vector<BinaryValue>& values() const;
	const OkPacket& ok_packet() const; // Can only be called after exhausted
	bool more_data() const { return state_ != State::exhausted; }
};

template <typename AsyncStream>
class PreparedStatement
{
	MysqlStream<AsyncStream>* stream_;
	int4 statement_id_;
	std::vector<ParamDefinition> params_;
	std::vector<ParamDefinition> columns_;

	BinaryResultset<AsyncStream> do_execute(const StmtExecute& message, int4 fetch_count);
public:
	static constexpr int4 MAX_FETCH_COUNT = std::numeric_limits<int4>::max();

	PreparedStatement(MysqlStream<AsyncStream>& stream, int4 statement_id,
			std::vector<ParamDefinition>&& params, std::vector<ParamDefinition>&& columns);
	PreparedStatement(const PreparedStatement&) = delete;
	PreparedStatement(PreparedStatement&&) = default;
	const PreparedStatement& operator=(const PreparedStatement&) = delete;
	PreparedStatement& operator=(PreparedStatement&&) = default;
	~PreparedStatement() = default;

	auto& next_layer() const { return *stream_; }
	int4 statement_id() const { return statement_id_; }
	const std::vector<ParamDefinition>& params() const { return params_; }
	const std::vector<ParamDefinition>& columns() const { return columns_; }

	template <typename... Params>
	BinaryResultset<AsyncStream> execute(Params&&... params) { return execute_with_cursor(MAX_FETCH_COUNT, std::forward<Params>(params)...); }

	template <typename... Params>
	BinaryResultset<AsyncStream> execute_with_cursor(int4 fetch_count, Params&&... params);

	void close();
	// Destructor should try to auto-close

	static PreparedStatement<AsyncStream> prepare(MysqlStream<AsyncStream>& stream, std::string_view query);
};

} // namespace mysql

#include "impl/prepared_statement_impl.hpp"

#endif /* INCLUDE_PREPARED_STATEMENT_HPP_ */
