// Copyright (c) 2022 Richard Hodges
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.

#include <boost/test/unit_test.hpp>

#include <boost/asem/mt.hpp>
#include <boost/asem/st.hpp>
#include <chrono>
#include <random>


#if defined(BOOST_ASEM_STANDALONE)

#include <asio.hpp>
#include <asio/coroutine.hpp>
#include <asio/detached.hpp>
#include <asio/yield.hpp>
#include <asio/append.hpp>
#include <asio/as_tuple.hpp>
#include <asio/experimental/parallel_group.hpp>

#else

#include <boost/asio.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/yield.hpp>
#include <boost/asio/append.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#endif

using namespace BOOST_ASEM_ASIO_NAMESPACE;
using namespace BOOST_ASEM_ASIO_NAMESPACE::experimental;
using namespace std::literals;
using namespace BOOST_ASEM_NAMESPACE;

using models = std::tuple<st, mt>;
template<typename T>
using context = typename std::conditional<
        std::is_same<st, T>::value,
        io_context,
        thread_pool
    >::type;

inline void run_impl(io_context & ctx)
{
    ctx.run();
}

inline void run_impl(thread_pool & ctx)
{
    ctx.join();
}

struct bot : BOOST_ASEM_ASIO_NAMESPACE::coroutine
{
    int n;
    st::semaphore &sem;
    std::chrono::milliseconds deadline;
    bot(int n, st::semaphore &sem, std::chrono::milliseconds deadline) : n(n), sem(sem), deadline(deadline) {}
    std::string ident = "bot " + std::to_string(n) + " : ";

    steady_timer timer{sem.get_executor()};


    std::chrono::steady_clock::time_point then;

    template<typename ... Args>
    void say(Args &&...args)
    {
        std::cout << ident;
        ((std::cout << args), ...);
    };

    void operator()(error_code = {}, std::size_t index = 0u)
    {
        reenter (this)
        {
            say("waiting up to ", deadline.count(), "ms\n");
            then = std::chrono::steady_clock::now();
            say("approaching semaphore\n");
            if (!sem.try_acquire())
            {
                timer.expires_after(deadline);
                say("waiting up to ", deadline.count(), "ms\n");

                yield experimental::make_parallel_group(
                        sem.async_acquire(deferred),
                        timer.async_wait(deferred)).
                        async_wait(experimental::wait_for_one(),
                                deferred([](std::array<std::size_t, 2u> seq, error_code ec_sem, error_code ec_tim)
                                {
                                    return deferred.values(ec_sem, seq[0]);
                                }))(std::move(*this));

                if (index == 0u)
                {
                    say("got bored waiting after ", deadline.count(), "ms\n");
                    BOOST_FAIL("semaphore timeout");
                    return ;
                }
                else
                {
                    say("semaphore acquired after ",
                        std::chrono::duration_cast< std::chrono::milliseconds >(
                                std::chrono::steady_clock::now() - then)
                                .count(),
                        "ms\n");
                }

            }
            else
                say("semaphore acquired immediately\n");

            timer.expires_after(500ms);
            yield timer.async_wait(std::move(*this));
            say("work done\n");

            sem.release();
            say("passed semaphore\n");
        }
    }
};

BOOST_AUTO_TEST_SUITE(basic_semaphore_test)

BOOST_AUTO_TEST_CASE_TEMPLATE(value, T, models)
{
    context<T> ioc;
    typename T::semaphore sem{ioc.get_executor(), 0};

    BOOST_CHECK_EQUAL(sem.value(), 0);

    sem.release();
    BOOST_CHECK_EQUAL(sem.value(), 1);
    sem.release();
    BOOST_CHECK_EQUAL(sem.value(), 2);

    sem.try_acquire();
    BOOST_CHECK_EQUAL(sem.value(), 1);

    sem.try_acquire();
    BOOST_CHECK_EQUAL(sem.value(), 0);

    sem.async_acquire(detached);
    BOOST_CHECK_EQUAL(sem.value(), -1);
    sem.async_acquire(detached);
    BOOST_CHECK_EQUAL(sem.value(), -2);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(random_sem, T, models)
{
    auto ioc  = context<T>{};
    auto sem  = typename T::semaphore(ioc.get_executor(), 10);
    auto rng  = std::random_device();
    auto ss   = std::seed_seq { rng(), rng(), rng(), rng(), rng() };
    auto eng  = std::default_random_engine(ss);
    auto dist = std::uniform_int_distribution< unsigned int >(1000, 10000);

    auto random_time = [&eng, &dist]
    { return std::chrono::milliseconds(dist(eng)); };
    for (int i = 0; i < 100; i += 2)
        post(ioc, bot(i, sem, random_time()));
    run_impl(ioc);
}

BOOST_AUTO_TEST_SUITE_END()