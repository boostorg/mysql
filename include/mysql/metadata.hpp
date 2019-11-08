#ifndef INCLUDE_MYSQL_METADATA_HPP_
#define INCLUDE_MYSQL_METADATA_HPP_

#include "mysql/impl/messages.hpp"
#include "mysql/impl/basic_types.hpp"

namespace mysql
{

class field_metadata
{
	detail::msgs::column_definition msg_;

	bool flag_set(std::uint16_t flag) const noexcept { return msg_.flags.value & flag; }
public:
	field_metadata() = default;
	field_metadata(const detail::msgs::column_definition& msg) noexcept: msg_(msg) {};

	std::string_view database() const noexcept { return msg_.schema.value; }
	std::string_view table() const noexcept { return msg_.table.value; }
	std::string_view original_table() const noexcept { return msg_.org_table.value; }
	std::string_view field_name() const noexcept { return msg_.name.value; }
	std::string_view original_field_name() const noexcept { return msg_.org_name.value; }
	collation field_collation() const noexcept { return msg_.character_set; }
	unsigned column_length() const noexcept { return msg_.column_length.value; }
	field_type type() const noexcept { return msg_.type; }
	unsigned decimals() const noexcept { return msg_.decimals.value; }
	bool is_not_null() const noexcept { return flag_set(detail::column_flags::not_null); }
	bool is_primary_key() const noexcept { return flag_set(detail::column_flags::pri_key); }
	bool is_unique_key() const noexcept { return flag_set(detail::column_flags::unique_key); }
	bool is_multiple_key() const noexcept { return flag_set(detail::column_flags::multiple_key); }
	bool is_blob() const noexcept { return flag_set(detail::column_flags::blob); }
	bool is_unsigned() const noexcept { return flag_set(detail::column_flags::unsigned_); }
	bool is_zerofill() const noexcept { return flag_set(detail::column_flags::zerofill); }
	bool is_binary() const noexcept { return flag_set(detail::column_flags::binary); }
	bool is_enum() const noexcept { return flag_set(detail::column_flags::enum_); }
	bool is_auto_increment() const noexcept { return flag_set(detail::column_flags::auto_increment); }
	bool is_timestamp() const noexcept { return flag_set(detail::column_flags::timestamp); }
	bool is_set() const noexcept { return flag_set(detail::column_flags::set); }
	bool has_no_default_value() const noexcept { return flag_set(detail::column_flags::no_default_value); }
	bool is_set_to_now_on_update() const noexcept { return flag_set(detail::column_flags::on_update_now); }
};

template <typename Allocator>
class owning_field_metadata : public field_metadata
{
	detail::bytestring<Allocator> msg_buffer_;

	bool flag_set(std::uint16_t flag) const noexcept { return msg_.flags.value & flag; }
public:
	owning_field_metadata() = default;
	owning_field_metadata(detail::bytestring<Allocator>&& buffer, const detail::msgs::column_definition& msg):
		field_metadata(msg), msg_buffer_(std::move(buffer)) {};
	owning_field_metadata(const owning_field_metadata&) = delete;
	owning_field_metadata(owning_field_metadata&&) = default;
	owning_field_metadata& operator=(const owning_field_metadata&) = delete;
	owning_field_metadata& operator=(owning_field_metadata&&) = default;
	~owning_field_metadata() = default;
};

template <typename Allocator>
class resultset_metadata
{
	std::vector<owning_field_metadata<Allocator>> fields_;
public:
	const auto& fields() const noexcept { return fields_; }
};

}



#endif /* INCLUDE_MYSQL_METADATA_HPP_ */
