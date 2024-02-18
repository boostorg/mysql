//
// Copyright (c) 2019-2024 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_ALGO_TEST_HPP
#define BOOST_MYSQL_TEST_UNIT_INCLUDE_TEST_UNIT_ALGO_TEST_HPP

#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/next_action.hpp>
#include <boost/mysql/impl/internal/sansio/sansio_algorithm.hpp>

#include <boost/asio/error.hpp>
#include <boost/config.hpp>
#include <boost/test/unit_test.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "test_common/assert_buffer_equals.hpp"
#include "test_common/create_diagnostics.hpp"
#include "test_common/printing.hpp"
#include "test_unit/printing.hpp"

namespace boost {
namespace mysql {
namespace test {

class BOOST_ATTRIBUTE_NODISCARD algo_test
{
    struct step_t
    {
        detail::next_action::type_t type;
        std::vector<std::uint8_t> bytes;
        error_code result;
    };

    std::vector<step_t> steps_;

    void handle_read(const step_t& op, detail::connection_state_data& st)
    {
        if (!op.result)
        {
            std::size_t bytes_transferred = 0;
            while (!st.reader.done() && bytes_transferred < op.bytes.size())
            {
                st.reader.prepare_buffer();
                auto buff = st.reader.buffer();
                std::size_t size_to_copy = (std::min)(op.bytes.size() - bytes_transferred, buff.size());
                std::memcpy(buff.data(), op.bytes.data() + bytes_transferred, size_to_copy);
                bytes_transferred += size_to_copy;
                st.reader.resume(size_to_copy);
            }
            BOOST_TEST_REQUIRE(st.reader.done());
            BOOST_TEST_REQUIRE(st.reader.error() == error_code());
        }
    }

    void handle_write(const step_t& op, detail::connection_state_data& st)
    {
        // Multi-frame messages are not supported by these tests (they don't add anything)
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(st.writer.current_chunk(), op.bytes);
    }

    algo_test& add_step(detail::next_action::type_t act_type, std::vector<std::uint8_t> bytes, error_code ec)
    {
        steps_.push_back(step_t{act_type, std::move(bytes), ec});
        return *this;
    }

    detail::next_action run_algo_until_step(detail::any_algo_ref algo, std::size_t num_steps_to_run)
    {
        assert(num_steps_to_run <= num_steps());

        // Start the op
        auto act = algo.resume(error_code());

        // Go through the requested steps
        for (std::size_t i = 0; i < num_steps_to_run; ++i)
        {
            BOOST_TEST_CONTEXT("Step " << i)
            {
                const auto& step = steps_[i];
                BOOST_TEST_REQUIRE(act.type() == step.type);
                if (step.type == detail::next_action::type_t::read)
                    handle_read(step, algo.get().conn_state());
                else if (step.type == detail::next_action::type_t::write)
                    handle_write(step, algo.get().conn_state());
                // Other actions don't need any handling

                act = algo.resume(step.result);
            }
        }

        return act;
    }

    std::size_t num_steps() const noexcept { return steps_.size(); }

    void check_impl(detail::any_algo_ref algo, error_code expected_ec = {})
    {
        // Run the op until completion
        auto act = run_algo_until_step(algo, steps_.size());

        // Check that we've finished
        BOOST_TEST_REQUIRE(act.type() == detail::next_action::type_t::none);
        BOOST_TEST(act.error() == expected_ec);
    }

    void check_network_errors_impl(detail::any_algo_ref algo, std::size_t step_number)
    {
        assert(step_number < num_steps());

        // Run all the steps that shouldn't cause an error
        auto act = run_algo_until_step(algo, step_number);
        BOOST_TEST_REQUIRE(act.type() == steps_[step_number].type);

        // Trigger an error in the requested step
        act = algo.resume(asio::error::bad_descriptor);

        // The operation finished and returned the network error
        BOOST_TEST_REQUIRE(act.type() == detail::next_action::type_t::none);
        BOOST_TEST(act.error() == error_code(asio::error::bad_descriptor));
    }

public:
    algo_test() = default;

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_write(std::vector<std::uint8_t> bytes, error_code result = {})
    {
        return add_step(detail::next_action::type_t::write, std::move(bytes), result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_read(std::vector<std::uint8_t> result_bytes)
    {
        return add_step(detail::next_action::type_t::read, std::move(result_bytes), error_code());
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_read(error_code result)
    {
        return add_step(detail::next_action::type_t::read, {}, result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_ssl_handshake(error_code result = {})
    {
        return add_step(detail::next_action::type_t::ssl_handshake, {}, result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_ssl_shutdown(error_code result = {})
    {
        return add_step(detail::next_action::type_t::ssl_shutdown, {}, result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_close(error_code result = {})
    {
        return add_step(detail::next_action::type_t::close, {}, result);
    }

    template <class AlgoFixture>
    void check(AlgoFixture& fix, error_code expected_ec = {}, const diagnostics& expected_diag = {})
    {
        check_impl(fix.algo, expected_ec);
        BOOST_TEST(fix.diag == expected_diag);
    }

    template <class AlgoFixture>
    void check_network_errors()
    {
        for (std::size_t i = 0; i < num_steps(); ++i)
        {
            BOOST_TEST_CONTEXT("check_network_errors erroring at step " << i)
            {
                AlgoFixture fix;
                check_network_errors_impl(fix.algo, i);
                BOOST_TEST(fix.diag == diagnostics());
            }
        }
    }
};

struct algo_fixture_base
{
    detail::connection_state_data st{512};
    diagnostics diag{create_server_diag("Diagnostics not cleared")};
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
