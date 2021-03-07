//
// Copyright (c) 2019-2021 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

//[example_ssl
#include "boost/mysql/collation.hpp"
#include "boost/mysql/connection_params.hpp"
#include "boost/mysql/mysql.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <iostream>

#define ASSERT(expr) \
    if (!(expr)) \
    { \
        std::cerr << "Assertion failed: " #expr << std::endl; \
        exit(1); \
    }

// The CA file that signed the server's certificate
constexpr const char CA_PEM [] = R"%(-----BEGIN CERTIFICATE-----
MIIDZzCCAk+gAwIBAgIUWznm2UoxXw3j7HCcp9PpiayTvFQwDQYJKoZIhvcNAQEL
BQAwQjELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxDjAMBgNVBAoM
BW15c3FsMQ4wDAYDVQQDDAVteXNxbDAgFw0yMDA0MDQxNDMwMjNaGA8zMDE5MDgw
NjE0MzAyM1owQjELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxDjAM
BgNVBAoMBW15c3FsMQ4wDAYDVQQDDAVteXNxbDCCASIwDQYJKoZIhvcNAQEBBQAD
ggEPADCCAQoCggEBAN0WYdvsDb+a0TxOGPejcwZT0zvTrf921mmDUlrLN1Z0hJ/S
ydgQCSD7Q+6za4lTFZCXcvs52xvvS2gfC0yXyYLCT/jA4RQRxuF+/+w1gDWEbGk0
KzEpsBuKrEIvEaVdoS78SxInnW/aegshdrRRocp4JQ6KHsZgkLTxSwPfYSUmMUo0
cRO0Q/ak3VK8NP13A6ZFvZjrBxjS3cSw9HqilgADcyj1D4EokvfI1C9LrgwgLlZC
XVkjjBqqoMXGGlnXOEK+pm8bU68HM/QvMBkb1Amo8pioNaaYgqJUCP0Ch0iu1nUU
HtsWt6emXv0jANgIW0oga7xcT4MDGN/M+IRWLTECAwEAAaNTMFEwHQYDVR0OBBYE
FNxhaGwf5ePPhzK7yOAKD3VF6wm2MB8GA1UdIwQYMBaAFNxhaGwf5ePPhzK7yOAK
D3VF6wm2MA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAAoeJCAX
IDCFoAaZoQ1niI6Ac/cds8G8It0UCcFGSg+HrZ0YujJxWIruRCUG60Q2OAbEvn0+
uRpTm+4tV1Wt92WFeuRyqkomozx0g4CyfsxGX/x8mLhKPFK/7K9iTXM4/t+xQC4f
J+iRmPVsMKQ8YsHYiWVhlOMH9XJQiqERCB2kOKJCH6xkaF2k0GbM2sGgbS7Z6lrd
fsFTOIVx0VxLVsZnWX3byE9ghnDR5jn18u30Cpb/R/ShxNUGIHqRa4DkM5la6uZX
W1fpSW11JBSUv4WnOO0C2rlIu7UJWOROqZZ0OsybPRGGwagcyff2qVRuI2XFvAMk
OzBrmpfHEhF6NDU=
-----END CERTIFICATE-----
)%";


void print_employee(const boost::mysql::row& employee)
{
    std::cout << "Employee '"
              << employee.values()[0] << " "                   // first_name (type boost::string_view)
              << employee.values()[1] << "' earns "            // last_name  (type boost::string_view)
              << employee.values()[2] << " dollars yearly\n";  // salary     (type double)
}

void main_impl(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <username> <password>\n";
        exit(1);
    }

    /**
     * Connection parameters that tell us where and how to connect to the MySQL server.
     * There are two types of parameters:
     *   - TCP-level connection parameters, identifying the host and port to connect to.
     *   - MySQL level parameters: database credentials and schema to use.
     */
    boost::asio::ip::tcp::endpoint ep (
        boost::asio::ip::address_v4::loopback(), // host
        boost::mysql::default_port                         // port
    );
    boost::mysql::connection_params params (
        argv[1],               // username
        argv[2],               // password
        "boost_mysql_examples",// database to use; leave empty or omit the parameter for no database
        boost::mysql::collation::utf8_general_ci, // character set and collation to use (this is the default)
        boost::mysql::ssl_mode::require // require SSL; if the server doesn't support it, fail
    );

    boost::asio::io_context ctx;

    // By default, boost::mysql::tcp_connection will create internally a
    // boost::asio::ssl::context with the default options. We can override
    // this behavior by passing a boost::asio::ssl::context instance to
    // tcp_connection's constructor. This allows us to customize SSL behavior
    // (e.g. enabling certificate validation, adding trusted CAs...). We will
    // use this feature to validate the hostname in the server's certificate.
    boost::asio::ssl::context ssl_ctx (boost::asio::ssl::context::tls_client);

    // Check whether the server's certificate is valid and signed by a trusted CA.
    // If it's not, our handshake or connect operation will fail.
    ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer); 

    // Load a trusted CA, which was used to sign the server's certificate.
    // This will allow the signature verification to succeed in our example.
    // You will have to run your MySQL server with the test certificates
    // located under $BOOST_MYSQL_ROOT/tools/ssl/
    ssl_ctx.add_certificate_authority(boost::asio::buffer(CA_PEM));

    // We expect the server certificate's common name to be "mysql".
    // If it's not, the certificate will be rejected and handshake or connect will fail.
    ssl_ctx.set_verify_callback(boost::asio::ssl::host_name_verification("mysql"));

    // Pass in our pre-configured SSL context to the connection. Note that we
    // can create many connections out of a single context. We need to keep the
    // context alive until we finish using the connection.
    boost::mysql::tcp_connection conn (ssl_ctx, ctx);

    // Connect to the server. This operation will perform the SSL handshake as part of
    // it, and thus will fail if the certificate is found to be invalid.
    conn.connect(ep, params);

    // We can now use the connection as we would normally do.
    const char* sql = "SELECT first_name, last_name, salary FROM employee WHERE company_id = 'HGS'";
    boost::mysql::tcp_resultset result = conn.query(sql);
    std::vector<boost::mysql::row> employees = result.read_all();
    for (const auto& employee: employees)
    {
        print_employee(employee);
    }

    // Cleanup
    conn.close();
}

int main(int argc, char** argv)
{
    try
    {
        main_impl(argc, argv);
    }
    catch (const boost::system::system_error& err)
    {
        std::cerr << "Error: " << err.what() << ", error code: " << err.code() << std::endl;
        return 1;
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << std::endl;
        return 1;
    }
}

//]
