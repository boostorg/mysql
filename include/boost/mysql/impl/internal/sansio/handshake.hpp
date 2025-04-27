//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_HANDSHAKE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_HANDSHAKE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/mysql_collations.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/ok_view.hpp>

#include <boost/mysql/impl/internal/coroutine.hpp>
#include <boost/mysql/impl/internal/protocol/capabilities.hpp>
#include <boost/mysql/impl/internal/protocol/db_flavor.hpp>
#include <boost/mysql/impl/internal/protocol/deserialization.hpp>
#include <boost/mysql/impl/internal/protocol/serialization.hpp>
#include <boost/mysql/impl/internal/sansio/auth_plugin.hpp>
#include <boost/mysql/impl/internal/sansio/caching_sha2_password.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/mysql_native_password.hpp>

#include <boost/core/span.hpp>
#include <boost/system/result.hpp>
#include <boost/variant2/variant.hpp>

#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class any_authentication_plugin
{
    enum class type_t
    {
        mnp,
        csha2p
    };

    // Which authentication plugin are we using?
    type_t type_{type_t::mnp};

    // State for algorithms that require stateful exchanges.
    // mysql_native_password is stateless, so only caching_sha2_password has an entry here
    caching_sha2_password_algo csha2p_;

public:
    any_authentication_plugin() = default;

    // Emplaces the plugin and computes the first authentication response by hashing the password
    system::result<hashed_password> bootstrap_plugin(
        string_view plugin_name,
        string_view password,
        span<const std::uint8_t> challenge
    )
    {
        if (plugin_name == "mysql_native_password")
        {
            type_ = type_t::mnp;
            return mnp_hash_password(password, challenge);
        }
        else if (plugin_name == "caching_sha2_password")
        {
            type_ = type_t::csha2p;
            csha2p_ = caching_sha2_password_algo();  // Reset any leftover state, just in case
            return csha2p_hash_password(password, challenge);
        }
        else
        {
            return client_errc::unknown_auth_plugin;
        }
    }

    next_action resume(
        connection_state_data& st,
        boost::span<const std::uint8_t> server_data,
        string_view password,
        bool secure_channel,
        std::uint8_t& seqnum
    )
    {
        switch (type_)
        {
        case type_t::mnp:
            // This algorithm doesn't allow more data frames
            return error_code(client_errc::protocol_value_error);
        case type_t::csha2p: return csha2p_.resume(st, server_data, password, secure_channel, seqnum);
        default:
            BOOST_ASSERT(false);
            return next_action(client_errc::protocol_value_error);  // LCOV_EXCL_LINE
        }
    }

    string_view name() const
    {
        switch (type_)
        {
        case type_t::mnp: return "mysql_native_password";  // TODO: these constants are repeated
        case type_t::csha2p: return "caching_sha2_password";
        default: BOOST_ASSERT(false); return {};  // LCOV_EXCL_LINE
        }
    }
};

class handshake_algo
{
    int resume_point_{0};
    handshake_params hparams_;
    any_authentication_plugin plugin_;
    hashed_password hashed_password_;
    std::uint8_t sequence_number_{0};
    bool secure_channel_{false};

    static capabilities conditional_capability(bool condition, capabilities cap)
    {
        return condition ? cap : capabilities{};
    }

    static error_code process_capabilities(
        const handshake_params& params,
        const server_hello& hello,
        capabilities& negotiated_caps,
        bool transport_supports_ssl
    )
    {
        // The capabilities that we absolutely require. These are always set except in extremely old servers
        constexpr capabilities mandatory_capabilities =
            // We don't speak the older protocol
            capabilities::protocol_41 |

            // We only know how to deserialize the hello frame if this is set
            capabilities::plugin_auth |

            // Same as above
            capabilities::plugin_auth_lenenc_data |

            // This makes processing execute responses easier
            capabilities::deprecate_eof |

            // Used in MariaDB to signal 4.1 protocol. Always set in MySQL, too
            capabilities::secure_connection;

        // The capabilities that we support but don't require
        constexpr capabilities optional_capabilities = capabilities::multi_results |
                                                       capabilities::ps_multi_results;

        auto ssl = transport_supports_ssl ? params.ssl() : ssl_mode::disable;
        capabilities server_caps = hello.server_capabilities;
        capabilities required_caps = mandatory_capabilities |
                                     conditional_capability(
                                         !params.database().empty(),
                                         capabilities::connect_with_db
                                     ) |
                                     conditional_capability(
                                         params.multi_queries(),
                                         capabilities::multi_statements
                                     ) |
                                     conditional_capability(ssl == ssl_mode::require, capabilities::ssl);
        if (has_capabilities(required_caps, capabilities::ssl) &&
            !has_capabilities(server_caps, capabilities::ssl))
        {
            // This happens if the server doesn't have SSL configured. This special
            // error code helps users diagnosing their problem a lot (server_unsupported doesn't).
            return make_error_code(client_errc::server_doesnt_support_ssl);
        }
        else if (!has_capabilities(server_caps, required_caps))
        {
            return make_error_code(client_errc::server_unsupported);
        }
        negotiated_caps = server_caps & (required_caps | optional_capabilities |
                                         conditional_capability(ssl == ssl_mode::enable, capabilities::ssl));
        return error_code();
    }

    // Attempts to map the collection_id to a character set. We try to be conservative
    // here, since servers will happily accept unknown collation IDs, silently defaulting
    // to the server's default character set (often latin1, which is not Unicode).
    static character_set collation_id_to_charset(std::uint16_t collation_id)
    {
        switch (collation_id)
        {
        case mysql_collations::utf8mb4_bin:
        case mysql_collations::utf8mb4_general_ci: return utf8mb4_charset;
        case mysql_collations::ascii_general_ci:
        case mysql_collations::ascii_bin: return ascii_charset;
        default: return character_set{};
        }
    }

    // Once the handshake is processed, the capabilities are stored in the connection state
    bool use_ssl(const connection_state_data& st) const
    {
        return has_capabilities(st.current_capabilities, capabilities::ssl);
    }

    error_code process_hello(connection_state_data& st, diagnostics& diag, span<const std::uint8_t> buffer)
    {
        // Deserialize server hello
        server_hello hello{};
        auto err = deserialize_server_hello(buffer, hello, diag);
        if (err)
            return err;

        // Check capabilities
        capabilities negotiated_caps{};
        err = process_capabilities(hparams_, hello, negotiated_caps, st.tls_supported);
        if (err)
            return err;

        // Set capabilities, db flavor and connection ID
        st.current_capabilities = negotiated_caps;
        st.flavor = hello.server;
        st.connection_id = hello.connection_id;

        // If we're using SSL, mark the channel as secure
        secure_channel_ = secure_channel_ || use_ssl(st);

        // Emplace the authentication plugin and compute the first response
        auto hashed_password = plugin_.bootstrap_plugin(
            hello.auth_plugin_name,
            hparams_.password(),
            hello.auth_plugin_data.to_span()
        );
        if (hashed_password.has_error())
            return hashed_password.error();

        // Save it for later
        hashed_password_ = *hashed_password;
        return error_code();
    }

    // Response to that initial greeting
    ssl_request compose_ssl_request(const connection_state_data& st)
    {
        return ssl_request{
            st.current_capabilities,
            static_cast<std::uint32_t>(max_packet_size),
            hparams_.connection_collation(),
        };
    }

    login_request compose_login_request(const connection_state_data& st)
    {
        return login_request{
            st.current_capabilities,
            static_cast<std::uint32_t>(max_packet_size),
            hparams_.connection_collation(),
            hparams_.username(),
            hashed_password_.to_span(),
            hparams_.database(),
            plugin_.name(),
        };
    }

    // Processes auth_switch and auth_more_data messages, and leaves the result in auth_resp_
    next_action process_auth_switch(connection_state_data& st, auth_switch msg)
    {
        // Emplace the authentication plugin and compute the first response
        auto hashed_password = plugin_.bootstrap_plugin(msg.plugin_name, hparams_.password(), msg.auth_data);
        if (hashed_password.has_error())
            return hashed_password.error();

        // Serialize the response
        return st.write(auth_switch_response{hashed_password->to_span()}, sequence_number_);
    }

    void on_success(connection_state_data& st, const ok_view& ok)
    {
        st.status = connection_status::ready;
        st.backslash_escapes = ok.backslash_escapes();
        st.current_charset = collation_id_to_charset(hparams_.connection_collation());
    }

public:
    handshake_algo(handshake_algo_params params) noexcept
        : hparams_(params.hparams), secure_channel_(params.secure_channel)
    {
    }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        if (ec)
            return ec;

        handshake_server_response resp(error_code{});
        next_action act;

        switch (resume_point_)
        {
        case 0:
            // Handshake wipes out state, so no state checks are performed.
            // Setup
            st.reset();

            // Read server greeting
            BOOST_MYSQL_YIELD(resume_point_, 1, st.read(sequence_number_))

            // Process server greeting
            ec = process_hello(st, diag, st.reader.message());
            if (ec)
                return ec;

            // SSL
            if (use_ssl(st))
            {
                // Send SSL request
                BOOST_MYSQL_YIELD(resume_point_, 2, st.write(compose_ssl_request(st), sequence_number_))

                // SSL handshake
                BOOST_MYSQL_YIELD(resume_point_, 3, next_action::ssl_handshake())

                // Mark the connection as using ssl
                st.tls_active = true;
            }

            // Compose and send handshake response
            BOOST_MYSQL_YIELD(resume_point_, 4, st.write(compose_login_request(st), sequence_number_))

            // Receive the response
            BOOST_MYSQL_YIELD(resume_point_, 5, st.read(sequence_number_))

            // Process it
            resp = deserialize_handshake_server_response(st.reader.message(), st.flavor, diag);

            // Auth switches are only legal at this point. Handle the case here
            if (resp.type == handshake_server_response::type_t::auth_switch)
            {
                // Write our packet
                BOOST_MYSQL_YIELD(resume_point_, 6, process_auth_switch(st, resp.data.auth_sw))

                // Read another packet
                BOOST_MYSQL_YIELD(resume_point_, 7, st.read(sequence_number_))

                // Deserialize it
                resp = deserialize_handshake_server_response(st.reader.message(), st.flavor, diag);
            }

            // Now we will send/receive raw data packets from the server until an OK or error happens.
            // Packets requiring responses are auth_more_data packets
            while (resp.type == handshake_server_response::type_t::auth_more_data)
            {
                // Invoke the authentication plugin algorithm
                act = plugin_.resume(
                    st,
                    resp.data.more_data,
                    hparams_.password(),
                    secure_channel_,
                    sequence_number_
                );

                // Do what the plugin says
                if (act.type() == next_action_type::none)
                {
                    // The plugin signalled an error. Exit
                    BOOST_ASSERT(act.error());
                    return act;
                }
                else if (act.type() == next_action_type::write)
                {
                    // The plugin wants us to first write the message in the write buffer, then read
                    BOOST_MYSQL_YIELD(resume_point_, 8, act)
                    BOOST_MYSQL_YIELD(resume_point_, 9, st.read(sequence_number_))
                }
                else
                {
                    // The plugin wants us to read another packet
                    BOOST_ASSERT(act.type() == next_action_type::read);
                    BOOST_MYSQL_YIELD(resume_point_, 10, st.read(sequence_number_))
                }

                // If we got here, we've successfully read a packet. Deserialize it
                resp = deserialize_handshake_server_response(st.reader.message(), st.flavor, diag);
            }

            // If we got here, we've received a packet that terminates the algorithm
            if (resp.type == handshake_server_response::type_t::ok)
            {
                // Auth success, quit
                on_success(st, resp.data.ok);
                return next_action();
            }
            else if (resp.type == handshake_server_response::type_t::error)
            {
                // Error, quit
                return resp.data.err;
            }
            else
            {
                // Auth switches are no longer allowed at this point
                BOOST_ASSERT(resp.type == handshake_server_response::type_t::auth_switch);
                return error_code(client_errc::protocol_value_error);
            }
        }

        return next_action();
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
