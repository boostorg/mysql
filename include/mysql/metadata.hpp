#ifndef MYSQL_ASIO_METADATA_HPP
#define MYSQL_ASIO_METADATA_HPP

#include "mysql/impl/messages.hpp"
#include "mysql/impl/basic_types.hpp"
#include "mysql/field_type.hpp"

namespace mysql
{

class field_metadata
{
	detail::msgs::column_definition msg_;
	mutable field_type field_type_ { field_type::_not_computed };

	bool flag_set(std::uint16_t flag) const noexcept { return msg_.flags.value & flag; }
public:
	field_metadata() = default;
	field_metadata(const detail::msgs::column_definition& msg) noexcept: msg_(msg) {};

	std::string_view database() const noexcept { return msg_.schema.value; }
	std::string_view table() const noexcept { return msg_.table.value; }
	std::string_view original_table() const noexcept { return msg_.org_table.value; }
	std::string_view field_name() const noexcept { return msg_.name.value; }
	std::string_view original_field_name() const noexcept { return msg_.org_name.value; }
	unsigned column_length() const noexcept { return msg_.column_length.value; }
	detail::protocol_field_type protocol_type() const noexcept { return msg_.type; }
	field_type type() const noexcept;
	unsigned decimals() const noexcept { return msg_.decimals.value; }
	bool is_not_null() const noexcept { return flag_set(detail::column_flags::not_null); }
	bool is_primary_key() const noexcept { return flag_set(detail::column_flags::pri_key); }
	bool is_unique_key() const noexcept { return flag_set(detail::column_flags::unique_key); }
	bool is_multiple_key() const noexcept { return flag_set(detail::column_flags::multiple_key); }
	bool is_unsigned() const noexcept { return flag_set(detail::column_flags::unsigned_); }
	bool is_zerofill() const noexcept { return flag_set(detail::column_flags::zerofill); }
	bool is_auto_increment() const noexcept { return flag_set(detail::column_flags::auto_increment); }
	bool has_no_default_value() const noexcept { return flag_set(detail::column_flags::no_default_value); }
	bool is_set_to_now_on_update() const noexcept { return flag_set(detail::column_flags::on_update_now); }
};

namespace detail
{

class resultset_metadata
{
	std::vector<bytestring> buffers_;
	std::vector<field_metadata> fields_;
public:
	resultset_metadata() = default;
	resultset_metadata(std::vector<bytestring>&& buffers, std::vector<field_metadata>&& fields):
		buffers_(std::move(buffers)), fields_(std::move(fields)) {};
	resultset_metadata(const resultset_metadata&) = delete;
	resultset_metadata(resultset_metadata&&) = default;
	resultset_metadata& operator=(const resultset_metadata&) = delete;
	resultset_metadata& operator=(resultset_metadata&&) = default;
	~resultset_metadata() = default;
	const auto& fields() const noexcept { return fields_; }
};

} // detail
} // mysql

#include "mysql/impl/metadata.hpp"

#endif