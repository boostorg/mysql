#ifndef TEST_INTEGRATION_METADATA_VALIDATOR_HPP_
#define TEST_INTEGRATION_METADATA_VALIDATOR_HPP_

#include "mysql/metadata.hpp"
#include <vector>

namespace mysql
{
namespace test
{

class meta_validator
{
public:
	using flag_getter = bool (field_metadata::*)() const noexcept;
	meta_validator(std::string table, std::string field, field_type type,
			std::vector<flag_getter> flags={}, unsigned decimals=0, collation col=collation::binary):
		table_(std::move(table)), org_table_(table_), field_(std::move(field)), org_field_(field_),
		decimals_(decimals), type_(type), col_(col), flags_(std::move(flags))
	{
	}
	meta_validator(std::string table, std::string field, field_type type, collation col,
			std::vector<flag_getter> flags={}, unsigned decimals=0):
		table_(std::move(table)), org_table_(table_), field_(std::move(field)), org_field_(field_),
		decimals_(decimals), type_(type), col_(col), flags_(std::move(flags))
	{
	}
	meta_validator(std::string table, std::string org_table,
			std::string field, std::string org_field, field_type type, collation col,
			std::vector<flag_getter> flags={}, unsigned decimals=0):
		table_(std::move(table)), org_table_(std::move(org_table)),
		field_(std::move(field)), org_field_(std::move(org_field)),
		decimals_(decimals), type_(type), col_(col), flags_(std::move(flags))
	{
	}
	void validate(const field_metadata& value) const;
private:
	std::string table_;
	std::string org_table_;
	std::string field_;
	std::string org_field_;
	unsigned decimals_;
	field_type type_;
	collation col_;
	std::vector<flag_getter> flags_;
};

void validate_meta(const std::vector<field_metadata>& actual, const std::vector<meta_validator>& expected);

}
}



#endif /* TEST_INTEGRATION_METADATA_VALIDATOR_HPP_ */
