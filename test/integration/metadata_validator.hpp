//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_INTEGRATION_METADATA_VALIDATOR_HPP
#define BOOST_MYSQL_TEST_INTEGRATION_METADATA_VALIDATOR_HPP

#include <boost/mysql/metadata.hpp>
#include <vector>

namespace boost {
namespace mysql {
namespace test {

class meta_validator
{
public:
    using flag_getter = bool (field_metadata::*)() const;
    meta_validator(std::string table, std::string field, field_type type,
            std::vector<flag_getter> flags={}, unsigned decimals=0,
            std::vector<flag_getter> ignore_flags={}):
        table_(std::move(table)), org_table_(table_), field_(std::move(field)), org_field_(field_),
        decimals_(decimals), type_(type), flags_(std::move(flags)),
        ignore_flags_(std::move(ignore_flags))
    {
    }
    meta_validator(std::string table, std::string org_table,
            std::string field, std::string org_field, field_type type,
            std::vector<flag_getter> flags={}, unsigned decimals=0,
            std::vector<flag_getter> ignore_flags={}):
        table_(std::move(table)), org_table_(std::move(org_table)),
        field_(std::move(field)), org_field_(std::move(org_field)),
        decimals_(decimals), type_(type), flags_(std::move(flags)),
        ignore_flags_(std::move(ignore_flags))
    {
    }
    void validate(const field_metadata& value) const;
    field_type type() const noexcept { return type_; }
private:
    std::string table_;
    std::string org_table_;
    std::string field_;
    std::string org_field_;
    unsigned decimals_;
    field_type type_;
    std::vector<flag_getter> flags_;
    std::vector<flag_getter> ignore_flags_;
};

void validate_meta(const std::vector<field_metadata>& actual, const std::vector<meta_validator>& expected);

} // test
} // mysql
} // boost



#endif /* TEST_INTEGRATION_METADATA_VALIDATOR_HPP_ */
