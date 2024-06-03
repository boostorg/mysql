//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/mysql/detail/pipeline.hpp>

#include <ostream>

#include "test_unit/operators/character_set.hpp"
#include "test_unit/operators/pipeline.hpp"
#include "test_unit/printing.hpp"

using namespace boost::mysql::detail;

// character set
bool boost::mysql::operator==(character_set lhs, character_set rhs) { return lhs.name == rhs.name; }

std::ostream& boost::mysql::operator<<(std::ostream& os, character_set v)
{
    return os << "character_set(\"" << v.name << "\")";
}

// pipeline_stage_kind
static const char* to_string(pipeline_stage_kind v)
{
    switch (v)
    {
    case pipeline_stage_kind::execute: return "pipeline_stage_kind::execute";
    case pipeline_stage_kind::prepare_statement: return "pipeline_stage_kind::prepare_statement";
    case pipeline_stage_kind::close_statement: return "pipeline_stage_kind::close_statement";
    case pipeline_stage_kind::reset_connection: return "pipeline_stage_kind::reset_connection";
    case pipeline_stage_kind::set_character_set: return "pipeline_stage_kind::set_character_set";
    case pipeline_stage_kind::ping: return "pipeline_stage_kind::ping";
    default: return "<unknown pipeline_stage_kind>";
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, pipeline_stage_kind v)
{
    return os << to_string(v);
}

// pipeline_request_stage
bool boost::mysql::detail::operator==(const pipeline_request_stage& lhs, const pipeline_request_stage& rhs)
{
    if (lhs.kind != rhs.kind || lhs.seqnum != rhs.seqnum)
        return false;
    switch (lhs.kind)
    {
    case pipeline_stage_kind::execute: return lhs.stage_specific.enc == rhs.stage_specific.enc;
    case pipeline_stage_kind::set_character_set:
        return lhs.stage_specific.charset == rhs.stage_specific.charset;
    default: return true;
    }
}

std::ostream& boost::mysql::detail::operator<<(std::ostream& os, pipeline_request_stage v)
{
    os << "pipeline_request_stage{ .kind = " << v.kind << ", .seqnum = " << +v.seqnum;
    switch (v.kind)
    {
    case pipeline_stage_kind::execute: os << ", .enc = " << v.stage_specific.enc; break;
    case pipeline_stage_kind::set_character_set: os << ", .charset = " << v.stage_specific.charset; break;
    default: break;
    }
    return os << " }";
}