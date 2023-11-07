//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_ANY_CONNECTION_HPP
#define BOOST_MYSQL_ANY_CONNECTION_HPP

#include <boost/mysql/buffer_params.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/connection_base.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/any_stream.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/throw_on_error_loc.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/variant2/variant.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace mysql {

class any_connection : public connection_base<asio::any_io_executor>
{
public:
    // TODO: maybe a "params struct"?
    any_connection(boost::asio::any_io_executor ex) : any_connection(std::move(ex), nullptr, buffer_params())
    {
    }
    any_connection(boost::asio::any_io_executor ex, const buffer_params& buff_params)
        : any_connection(std::move(ex), nullptr, buff_params)
    {
    }
    any_connection(boost::asio::any_io_executor ex, asio::ssl::context& ssl_ctx)
        : any_connection(std::move(ex), &ssl_ctx, buffer_params())
    {
    }
    any_connection(
        boost::asio::any_io_executor ex,
        asio::ssl::context& ssl_ctx,
        const buffer_params& buff_params
    )
        : any_connection(std::move(ex), &ssl_ctx, buff_params)
    {
    }

    // TODO: do we want to expose this?
    any_connection(asio::any_io_executor ex, asio::ssl::context* ctx, const buffer_params& buff)
        : base_type(buff, create_stream(std::move(ex), ctx))
    {
    }

    /**
     * \brief Move constructor.
     */
    any_connection(any_connection&& other) = default;

    /**
     * \brief Move assignment.
     */
    any_connection& operator=(any_connection&& rhs) = default;

#ifndef BOOST_MYSQL_DOXYGEN
    any_connection(const any_connection&) = delete;
    any_connection& operator=(const any_connection&) = delete;
#endif

    /// The executor type associated to this object.
    using executor_type = boost::asio::any_io_executor;

    /// Retrieves the executor associated to this object.
    executor_type get_executor() { return impl_.stream().get_executor(); }

    void connect(const connect_params& params, error_code& ec, diagnostics& diag)
    {
        impl_.connect_v2(params, ec, diag);
    }

    /// \copydoc connect
    void connect(const connect_params& params)
    {
        error_code err;
        diagnostics diag;
        connect(params, err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(const connect_params& params, CompletionToken&& token)
    {
        return async_connect(params, impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_connect(const connect_params& params, diagnostics& diag, CompletionToken&& token)
    {
        return impl_.async_connect_v2(params, diag, std::forward<CompletionToken>(token));
    }

    void close(error_code& err, diagnostics& diag)
    {
        this->impl_.run(this->impl_.make_params_close(diag), err);
    }

    /// \copydoc close
    void close()
    {
        error_code err;
        diagnostics diag;
        close(err, diag);
        detail::throw_on_error_loc(err, diag, BOOST_CURRENT_LOCATION);
    }

    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(CompletionToken&& token)
    {
        return async_close(impl_.shared_diag(), std::forward<CompletionToken>(token));
    }

    /// \copydoc async_close
    template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error_code)) CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error_code))
    async_close(diagnostics& diag, CompletionToken&& token)
    {
        return this->impl_.async_run(
            this->impl_.make_params_close(diag),
            std::forward<CompletionToken>(token)
        );
    }

private:
    using base_type = connection_base<asio::any_io_executor>;

    BOOST_MYSQL_DECL
    static std::unique_ptr<detail::any_stream> create_stream(
        asio::any_io_executor ex,
        asio::ssl::context* ctx
    );
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/any_connection.ipp>
#endif

#endif
