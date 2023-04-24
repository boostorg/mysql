//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_stored_procedures

#include <boost/mysql/static_execution_state.hpp>
#include <boost/mysql/tcp_ssl.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/describe/class.hpp>
#include <boost/variant2/variant.hpp>

#include <array>
#include <iostream>
#include <string>
#include <tuple>

namespace {

using empty = std::tuple<>;

// Types for the received rows
struct order
{
    std::int64_t id;
    std::string status;
};
BOOST_DESCRIBE_STRUCT(order, (), (id, status));

struct order_item
{
    std::int64_t id;
    std::int64_t quantity;
    std::int64_t unit_price;
};
BOOST_DESCRIBE_STRUCT(order_item, (), (id, quantity, unit_price));

struct product
{
    std::int64_t id;
    std::string short_name;
    boost::optional<std::string> description;
    std::int64_t price;
};
BOOST_DESCRIBE_STRUCT(product, (), (id, short_name, description, price));

void main_impl(int argc, char** argv)
{
    // Parse command line arguments
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password> <server-hostname> <order-id>\n";
        exit(1);
    }

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(ctx, ssl_ctx);

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    // Connection params
    boost::mysql::handshake_params params(
        argv[1],                         // username
        argv[2],                         // password
        "boost_mysql_stored_procedures"  // database to use
    );

    // Hostname resolution
    auto endpoints = resolver.resolve(argv[3], boost::mysql::default_port_string);

    // TCP and MySQL level connect
    conn.connect(*endpoints.begin(), params);

    // Do stuff
    auto stmt = conn.prepare_statement("CALL get_order(?)");

    boost::mysql::static_execution_state<order, order_item, empty> st;
    conn.start_execution(stmt.bind(argv[4]), st);

    // First resultset
    std::array<order, 1> ord;
    while (st.should_read_rows())
    {
        std::size_t read_rows = conn.read_some_rows(st, boost::span<order>(ord));
        for (std::size_t i = 0; i < read_rows; ++i)
        {
            std::cout << "Order: id=" << ord[i].id << ", status=" << ord[i].status << '\n';
        }
    }

    // Second resultset
    conn.read_resultset_head(st);
    std::array<order_item, 20> order_items;
    while (st.should_read_rows())
    {
        std::size_t read_rows = conn.read_some_rows(st, boost::span<order_item>(order_items));
        for (std::size_t i = 0; i < read_rows; ++i)
        {
            const auto& item = order_items[i];
            std::cout << "  Line item: id=" << item.id << ", quantity=" << item.quantity
                      << ", unit_price=" << item.unit_price / 100.0 << "$\n";
        }
    }

    // Third, final resultset
    conn.read_resultset_head(st);

    // Close the connection
    conn.close();
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::mysql::error_with_diagnostics& err)
    {
        // Some errors include additional diagnostics, like server-provided error messages.
        // If a store procedure fails (e.g. because a SIGNAL statement was executed), an error
        // like this will be raised.
        // Security note: diagnostics::server_message may contain user-supplied values (e.g. the
        // field value that caused the error) and is encoded using to the connection's encoding
        // (UTF-8 by default). Treat is as untrusted input.
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << '\n'
                  << "Server diagnostics: " << err.get_diagnostics().server_message() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]
