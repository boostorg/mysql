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

class BinaryResultset
{
	enum class State { initial, data_available, exhausted };

	MysqlStream* stream_;
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
	BinaryResultset(MysqlStream& stream, int4 stmt_id, int4 fetch_count):
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

class PreparedStatement
{
	MysqlStream* stream_;
	int4 statement_id_;
	std::vector<ParamDefinition> params_;
	std::vector<ParamDefinition> columns_;

	BinaryResultset do_execute(const StmtExecute& message, int4 fetch_count);
public:
	static constexpr int4 MAX_FETCH_COUNT = std::numeric_limits<int4>::max();

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
	BinaryResultset execute(Params&&... params) { return execute_with_cursor(MAX_FETCH_COUNT, std::forward<Params>(params)...); }

	template <typename... Params>
	BinaryResultset execute_with_cursor(int4 fetch_count, Params&&... params);

	void close();
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
	output.num_params = static_cast<int1>(num_params);
	output.new_params_bind_flag = 1;
	output.param_values.resize(num_params);
	fill_execute_msg_impl(output.param_values.begin(), std::forward<Args>(args)...);
}

}

}


template <typename... Params>
mysql::BinaryResultset mysql::PreparedStatement::execute_with_cursor(
	int4 fetch_count,
	Params&&... actual_params
)
{
	int1 flags = fetch_count == MAX_FETCH_COUNT ? CURSOR_TYPE_NO_CURSOR : CURSOR_TYPE_READ_ONLY;
	StmtExecute message
	{
		statement_id_,
		flags
	};
	detail::fill_execute_msg(message, params_.size(), std::forward<Params>(actual_params)...);
	return do_execute(message, fetch_count);
}


#endif /* INCLUDE_PREPARED_STATEMENT_HPP_ */
