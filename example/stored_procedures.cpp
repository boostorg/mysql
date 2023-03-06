//
// Copyright (c) 2019-2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_prepared_statements

#include <boost/mysql.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/variant2/variant.hpp>

#include <iostream>
#include <string>
#include <tuple>

using boost::mysql::string_view;

namespace {

struct get_products_args
{
    std::string search;
};

struct create_order_args
{
};

struct get_order_args
{
    std::int64_t order_id;
};

struct get_orders_args
{
};

struct add_line_item_args
{
    std::int64_t order_id;
    std::int64_t product_id;
    std::int64_t quantity;
};

struct remove_line_item_args
{
    std::int64_t line_item_id;
};

struct checkout_order_args
{
    std::int64_t order_id;
};

using any_command = boost::variant2::variant<
    get_products_args,
    get_order_args,
    get_orders_args,
    create_order_args,
    add_line_item_args,
    remove_line_item_args,
    checkout_order_args>;

struct cmdline_args
{
    string_view username;
    string_view password;
    string_view host;
    any_command cmd;
};

[[noreturn]] void usage(string_view program_name)
{
    std::cerr << "Usage: " << program_name << " <username> <password> <server-hostname> <command> args...\n"
              << "Available commands:\n"
                 "    get-products <search-term>\n"
                 "    create-order\n"
                 "    get-order <order-id>\n"
                 "    add-line-item <order-id> <product-id> <quantity>\n"
                 "    remove-line-item <line-item-id>\n"
                 "    checkout-order <order-id>"
              << std::endl;
    exit(1);
}

any_command parse_command(string_view program_name, string_view cmd_name, int argc_rest, char** argv_rest)
{
    if (cmd_name == "get-products")
    {
        if (argc_rest != 1)
        {
            usage(program_name);
        }
        return get_products_args{argv_rest[0]};
    }
    else if (cmd_name == "create-order")
    {
        if (argc_rest != 0)
        {
            usage(program_name);
        }
        return create_order_args{};
    }
    else if (cmd_name == "get-order")
    {
        if (argc_rest != 1)
        {
            usage(program_name);
        }
        return get_order_args{std::stoi(argv_rest[0])};
    }
    else if (cmd_name == "get-orders")
    {
        if (argc_rest != 0)
        {
            usage(program_name);
        }
        return get_orders_args{};
    }
    else if (cmd_name == "add-line-item")
    {
        if (argc_rest != 3)
        {
            usage(program_name);
        }
        return add_line_item_args{
            std::stoi(argv_rest[0]),
            std::stoi(argv_rest[1]),
            std::stoi(argv_rest[2]),
        };
    }
    else if (cmd_name == "remove-line-item")
    {
        if (argc_rest != 1)
        {
            usage(program_name);
        }
        return remove_line_item_args{
            std::stoi(argv_rest[0]),
        };
    }
    else if (cmd_name == "checkout-order")
    {
        if (argc_rest != 1)
        {
            usage(program_name);
        }
        return checkout_order_args{std::stoi(argv_rest[0])};
    }
    else
    {
        usage(program_name);
    }
}

cmdline_args parse_cmdline_args(int argc, char** argv)
{
    if (argc < 5)
    {
        usage(argv[0]);
    }
    return cmdline_args{
        argv[1],
        argv[2],
        argv[3],
        parse_command(argv[0], argv[4], argc - 5, argv + 5),
    };
}

struct visitor
{
    boost::mysql::tcp_ssl_connection& conn;

    static void print_order(const boost::mysql::results& result, string_view phrase)
    {
        // First resultset: order information. Always a single row
        auto order = result.at(0).rows().at(0);
        std::cout << phrase << ": id=" << order.at(0) << ", status=" << order.at(1) << '\n';

        // Second resultset: all order line items
        auto line_items = result.at(1).rows();
        if (line_items.empty())
        {
            std::cout << "No line items\n";
        }
        else
        {
            for (auto item : line_items)
            {
                std::cout << "  Line item: id=" << item.at(0) << ", quantity=" << item.at(1)
                          << ", unit price=" << item.at(2).as_int64() / 100.0 << "$\n";
            }
        }
    }

    void operator()(const get_products_args& args) const
    {
        boost::mysql::results result;
        auto stmt = conn.prepare_statement("CALL get_products(?)");
        conn.execute_statement(stmt, std::make_tuple(args.search), result);
        auto products = result.front();
        std::cout << "Your search returned the following products:\n";
        for (auto product : products.rows())
        {
            std::cout << "* ID: " << product.at(0) << '\n'
                      << "  Short name: " << product.at(1) << '\n'
                      << "  Description: " << product.at(2) << '\n'
                      << "  Price: " << product.at(3).as_int64() / 100.0 << "$" << std::endl;
        }
        std::cout << std::endl;
    }

    void operator()(const create_order_args&) const
    {
        boost::mysql::results result;
        auto stmt = conn.prepare_statement("CALL create_order(?)");
        conn.execute_statement(stmt, std::make_tuple(nullptr), result);
        auto order_id = result.out_params().at(0).as_int64();
        std::cout << "Created order: " << order_id << std::endl;
    }

    void operator()(const get_order_args& args) const
    {
        boost::mysql::results result;
        auto stmt = conn.prepare_statement("CALL get_order(?)");
        conn.execute_statement(stmt, std::make_tuple(args.order_id), result);
        print_order(result, "Retrieved order");
    }

    void operator()(const get_orders_args&) const
    {
        boost::mysql::results result;
        conn.query("CALL get_orders()", result);
        auto orders = result.front().rows();
        if (orders.empty())
        {
            std::cout << "No orders found" << std::endl;
        }
        else
        {
            for (auto order : result.front().rows())
            {
                std::cout << "Order with id=" << order.at(0) << ", status=" << order.at(1) << '\n';
            }
        }
    }

    void operator()(const add_line_item_args& args) const
    {
        boost::mysql::results result;
        auto stmt = conn.prepare_statement("CALL add_line_item(?, ?, ?, ?)");
        conn.execute_statement(
            stmt,
            std::make_tuple(args.order_id, args.product_id, args.quantity, nullptr),
            result
        );
        print_order(result, "Added line item to order");

        // The output parameter is the last resultset
        auto new_line_item_id = result.out_params().at(0).as_int64();
        std::cout << "The newly created line item ID is: " << new_line_item_id << std::endl;
    }

    void operator()(const remove_line_item_args& args) const
    {
        boost::mysql::results result;
        auto stmt = conn.prepare_statement("CALL remove_line_item(?)");
        conn.execute_statement(stmt, std::make_tuple(args.line_item_id), result);
        print_order(result, "Removed line item from order");
    }

    void operator()(const checkout_order_args& args) const
    {
        boost::mysql::results result;
        auto stmt = conn.prepare_statement("CALL checkout_order(?, ?)");
        conn.execute_statement(stmt, std::make_tuple(args.order_id, nullptr), result);
        print_order(result, "Checked out order");
        std::cout << "The total amount to pay is: " << result.out_params().at(0).as_int64() / 100.0 << "$"
                  << std::endl;
    }
};

void main_impl(int argc, char** argv)
{
    // Parse command line arguments
    auto args = parse_cmdline_args(argc, argv);

    // I/O context and connection. We use SSL because MySQL 8+ default settings require it.
    boost::asio::io_context ctx;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
    boost::mysql::tcp_ssl_connection conn(ctx, ssl_ctx);

    // Resolver for hostname resolution
    boost::asio::ip::tcp::resolver resolver(ctx.get_executor());

    // Connection params
    boost::mysql::handshake_params params(
        args.username,          // username
        args.password,          // password
        "boost_mysql_examples"  // database to use; leave empty or omit the parameter for no
                                // database
    );
    params.set_multi_results(true);

    // Hostname resolution
    auto endpoints = resolver.resolve(args.host, boost::mysql::default_port_string);

    // TCP and MySQL level connect
    conn.connect(*endpoints.begin(), params);

    // Execute the command
    boost::variant2::visit(visitor{conn}, args.cmd);

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
