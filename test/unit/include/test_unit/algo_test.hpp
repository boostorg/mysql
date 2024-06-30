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

#include <boost/mysql/detail/next_action.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/asio/error.hpp>
#include <boost/config.hpp>
#include <boost/core/span.hpp>
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

// A type-erased reference to an algorithm
class any_algo_ref
{
    template <class Algo>
    static detail::next_action do_resume(void* self, detail::connection_state_data& st, error_code ec)
    {
        return static_cast<Algo*>(self)->resume(st, ec);
    }

    using fn_t = detail::next_action (*)(void*, detail::connection_state_data&, error_code);

    void* algo_{};
    fn_t fn_{};

public:
    template <class Algo, class = typename std::enable_if<!std::is_same<Algo, any_algo_ref>::value>::type>
    any_algo_ref(Algo& algo) noexcept : algo_(&algo), fn_(&do_resume<Algo>)
    {
    }

    detail::next_action resume(detail::connection_state_data& st, error_code ec)
    {
        return fn_(algo_, st, ec);
    }
};

class BOOST_ATTRIBUTE_NODISCARD algo_test
{
    struct step_t
    {
        detail::next_action_type type;
        std::vector<std::uint8_t> bytes;
        error_code result;
    };

    std::vector<step_t> steps_;

    static void handle_read(detail::connection_state_data& st, const step_t& op)
    {
        if (!op.result)
        {
            std::size_t bytes_transferred = 0;
            while (!st.reader.done() && bytes_transferred < op.bytes.size())
            {
                auto ec = st.reader.prepare_buffer();
                BOOST_TEST_REQUIRE(ec == error_code());
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

    static void handle_write(span<const std::uint8_t> actual_msg, const step_t& op)
    {
        BOOST_MYSQL_ASSERT_BUFFER_EQUALS(actual_msg, op.bytes);
    }

    algo_test& add_step(detail::next_action_type act_type, std::vector<std::uint8_t> bytes, error_code ec)
    {
        steps_.push_back(step_t{act_type, std::move(bytes), ec});
        return *this;
    }

    detail::next_action run_algo_until_step(
        detail::connection_state_data& st,
        any_algo_ref algo,
        std::size_t num_steps_to_run
    ) const
    {
        assert(num_steps_to_run <= num_steps());

        // Start the op
        auto act = algo.resume(st, error_code());

        // Go through the requested steps
        for (std::size_t i = 0; i < num_steps_to_run; ++i)
        {
            BOOST_TEST_CONTEXT("Step " << i)
            {
                const auto& step = steps_[i];
                BOOST_TEST_REQUIRE(act.type() == step.type);
                if (step.type == detail::next_action_type::read)
                    handle_read(st, step);
                else if (step.type == detail::next_action_type::write)
                    handle_write(act.write_args().buffer, step);
                // Other actions don't need any handling

                act = algo.resume(st, step.result);
            }
        }

        return act;
    }

    std::size_t num_steps() const { return steps_.size(); }

    void check_impl(detail::connection_state_data& st, any_algo_ref algo, error_code expected_ec = {}) const
    {
        // Run the op until completion
        auto act = run_algo_until_step(st, algo, steps_.size());

        // Check that we've finished
        BOOST_TEST_REQUIRE(act.type() == detail::next_action_type::none);
        BOOST_TEST(act.error() == expected_ec);
    }

    void check_network_errors_impl(
        detail::connection_state_data& st,
        any_algo_ref algo,
        std::size_t step_number
    ) const
    {
        assert(step_number < num_steps());

        // Run all the steps that shouldn't cause an error
        auto act = run_algo_until_step(st, algo, step_number);
        BOOST_TEST_REQUIRE(act.type() == steps_[step_number].type);

        // Trigger an error in the requested step
        act = algo.resume(st, asio::error::bad_descriptor);

        // The operation finished and returned the network error
        BOOST_TEST_REQUIRE(act.type() == detail::next_action_type::none);
        BOOST_TEST(act.error() == error_code(asio::error::bad_descriptor));
    }

public:
    algo_test() = default;

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_write(std::vector<std::uint8_t> bytes, error_code result = {})
    {
        return add_step(detail::next_action_type::write, std::move(bytes), result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_read(std::vector<std::uint8_t> result_bytes)
    {
        return add_step(detail::next_action_type::read, std::move(result_bytes), error_code());
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_read(error_code result) { return add_step(detail::next_action_type::read, {}, result); }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_ssl_handshake(error_code result = {})
    {
        return add_step(detail::next_action_type::ssl_handshake, {}, result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_ssl_shutdown(error_code result = {})
    {
        return add_step(detail::next_action_type::ssl_shutdown, {}, result);
    }

    BOOST_ATTRIBUTE_NODISCARD
    algo_test& expect_close(error_code result = {})
    {
        return add_step(detail::next_action_type::close, {}, result);
    }

    template <class AlgoFixture>
    void check(AlgoFixture& fix, error_code expected_ec = {}, const diagnostics& expected_diag = {}) const
    {
        check_impl(fix.st, fix.algo, expected_ec);
        BOOST_TEST(fix.diag == expected_diag);
    }

    template <class AlgoFixture>
    void check_network_errors() const
    {
        for (std::size_t i = 0; i < num_steps(); ++i)
        {
            BOOST_TEST_CONTEXT("check_network_errors erroring at step " << i)
            {
                AlgoFixture fix;
                check_network_errors_impl(fix.st, fix.algo, i);
                BOOST_TEST(fix.diag == diagnostics());
            }
        }
    }
};

struct algo_fixture_base
{
    detail::connection_state_data st{512};
    diagnostics diag;

    algo_fixture_base(diagnostics initial_diag = create_server_diag("Diagnostics not cleared"))
        : diag(std::move(initial_diag))
    {
        st.write_buffer.push_back(0xff);  // Check that we clear the write buffer at each step
    }
};

}  // namespace test
}  // namespace mysql
}  // namespace boost

#endif
