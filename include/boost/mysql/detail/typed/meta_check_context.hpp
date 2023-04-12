//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_TYPED_META_CHECK_CONTEXT_HPP
#define BOOST_MYSQL_DETAIL_TYPED_META_CHECK_CONTEXT_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/metadata.hpp>

#include <sstream>

namespace boost {
namespace mysql {
namespace detail {

class meta_check_context
{
    std::unique_ptr<std::ostringstream> errors_;
    std::size_t current_index_{};
    const metadata* meta_{};
    const char* cpp_type_name_{};

    std::ostringstream& error_stream()
    {
        if (!errors_)
            errors_.reset(new std::ostringstream);
        return *errors_;
    }

public:
    meta_check_context() = default;
    meta_check_context(const metadata* meta) : meta_(meta) {}
    const metadata& current_meta() const noexcept { return meta_[current_index_]; }
    void set_cpp_type_name(const char* v) noexcept { cpp_type_name_ = v; }
    void advance() noexcept { ++current_index_; }
    std::size_t current_index() const noexcept { return current_index_; }

    void add_error(const char* reason)
    {
        error_stream() << "Incompatible types for field in position " << current_index_ << ": C++ type "
                       << cpp_type_name_ << " is not compatible with DB type " << current_meta().type()
                       << ": " << reason << "\n";
    }

    bool has_errors() const noexcept { return errors_ != nullptr; }

    std::string errors() const
    {
        assert(errors_);
        return errors_->str();
    }

    error_code check_errors(diagnostics& diag) const
    {
        if (has_errors())
        {
            diagnostics_access::assign(diag, errors(), false);
            return client_errc::type_mismatch;
        }
        return error_code();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
