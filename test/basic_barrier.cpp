// Copyright (c) 2022 Klemens D. Morgenstern, Ricahrd Hodges
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/test/unit_test.hpp>

#include <boost/asem/lock_guard.hpp>
#include <boost/asem/mt.hpp>
#include <boost/asem/st.hpp>
#include <chrono>
#include <random>

#if !defined(BOOST_ASEM_STANDALONE)
namespace asio = boost::asio;
#include <boost/asio.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/yield.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#else
#include <asio.hpp>
#include <asio/compose.hpp>
#include <asio/yield.hpp>
#include <asio/experimental/parallel_group.hpp>
#endif

using namespace BOOST_ASEM_NAMESPACE;
using namespace net;
using namespace net::experimental;

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

template<typename T>
struct basic_barrier_main_impl
{
    std::atomic<int> done{0};
    typename T::barrier barrier;
    asio::steady_timer tim{barrier.get_executor()};
    basic_barrier_main_impl(net::any_io_executor exec) : barrier{exec, 5} {}

};

template<typename T>
struct basic_barrier_main : net::coroutine
{

    basic_barrier_main(net::any_io_executor exec)
        : impl_(std::make_unique<basic_barrier_main_impl<T>>(exec)) {}

    std::unique_ptr<basic_barrier_main_impl<T>> impl_;

    void operator()(error_code = {})
    {
        auto p = impl_.get();
        int val = impl_->done.load();
        reenter (this)
        {
            impl_->barrier.async_arrive([p](error_code ec){BOOST_CHECK(!ec); p->done |= 1;});
            impl_->barrier.async_arrive([p](error_code ec){BOOST_CHECK(!ec); p->done |= 2;});
            impl_->barrier.async_arrive([p](error_code ec){BOOST_CHECK(!ec); p->done |= 4;});
            impl_->barrier.async_arrive([p](error_code ec){BOOST_CHECK(!ec); p->done |= 8;});
            BOOST_CHECK_EQUAL(p->done, 0);
            yield asio::post(impl_->barrier.get_executor(), std::move(*this));
            BOOST_CHECK_EQUAL(p->done, 0);
            yield impl_->barrier.async_arrive(std::move(*this));
            impl_->tim.expires_after(std::chrono::milliseconds(10));
            yield impl_->tim.async_wait(std::move(*this));
            BOOST_CHECK_EQUAL(val, 15);
        }
    }


};

BOOST_AUTO_TEST_SUITE(basic_barrier_test)

BOOST_AUTO_TEST_CASE_TEMPLATE(random_barrier, T, models)
{
    context<T> ctx;
    net::post(ctx, basic_barrier_main<T>{ctx.get_executor()});
    run_impl(ctx);
}



BOOST_AUTO_TEST_CASE(rebind_barrier)
{
    asio::io_context ctx;
    auto res = asio::deferred.as_default_on(st::barrier{ctx.get_executor(), 4u});
    res = typename st::barrier::rebind_executor<io_context::executor_type>::other{ctx.get_executor(), 2u};

}


BOOST_AUTO_TEST_CASE(sync_barrier_st)
{
    asio::io_context ctx;
    st::barrier b{ctx.get_executor(), 4u};
    BOOST_CHECK_THROW(b.arrive(), system_error);

    st::barrier b2{ctx.get_executor(), 1u};
    BOOST_CHECK_NO_THROW(b2.arrive());
}


BOOST_AUTO_TEST_CASE(sync_barrier_m)
{
    asio::io_context ctx;
    mt::barrier b{ctx.get_executor(), 2u};

    asio::post(ctx, [&]{b.arrive();});

    std::thread thr{[&]{ctx.run();}};

    BOOST_CHECK_NO_THROW(b.arrive());
    thr.join();
}


BOOST_AUTO_TEST_CASE_TEMPLATE(shutdown_wp, T, models)
{
  asio::io_context ctx;
  auto smtx = std::make_shared<typename T::barrier>(ctx, 2);
  auto l =  [smtx](error_code ec) { BOOST_CHECK(false); };

  smtx->async_arrive(l);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(shutdown_, T, models)
{
  std::weak_ptr<typename T::barrier> wp;
  {
    io_context ctx;
    auto smtx = std::make_shared<typename T::barrier>(ctx, 2);
    wp = smtx;
    auto l =  [smtx](error_code ec) { BOOST_CHECK(false); };

    smtx->async_arrive(l);
  }

  BOOST_CHECK(wp.expired());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(cancel, T, models)
{
  context<T> ctx;
  auto smtx = std::make_shared<typename T::barrier>(ctx, 2);
  auto l =  [smtx](error_code ec) { BOOST_CHECK(ec == asio::error::operation_aborted); };

  asio::cancellation_signal sig;

  smtx->async_arrive(asio::bind_cancellation_slot(sig.slot(), l));

  asio::post(
      [&]
      {
        sig.emit(asio::cancellation_type::total);
      });

  run_impl(ctx);
}

BOOST_AUTO_TEST_SUITE_END()