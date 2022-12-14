// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_ASEM_MT_BASIC_BARRIER_HPP
#define BOOST_ASEM_MT_BASIC_BARRIER_HPP

#include <boost/asem/basic_barrier.hpp>
#include <boost/asem/detail/config.hpp>

#include <atomic>
#include <barrier>
#include <mutex>

BOOST_ASEM_BEGIN_NAMESPACE

namespace detail
{

template<>
struct barrier_impl<mt> : detail::service_member<mt>
{
    barrier_impl(net::execution_context & ctx,
                 std::ptrdiff_t init) : detail::service_member<mt>(ctx), init_(init) {}

    std::ptrdiff_t init_;
    std::atomic<std::ptrdiff_t> counter_{init_};

    BOOST_ASEM_DECL bool try_arrive();
    BOOST_ASEM_DECL void
    add_waiter(detail::wait_op *waiter) noexcept;

    void decrement()
    {
        counter_--;
    }
    BOOST_ASEM_DECL void arrive(error_code & ec);

    void shutdown() override
    {
      auto w = std::move(waiters_);
      w.shutdown();
    }

    auto internal_lock() -> std::unique_lock<std::mutex>
    {
        return std::unique_lock<std::mutex>{mtx_};
    }

    std::atomic<bool> locked_{false};
    std::mutex mtx_;
    detail::basic_bilist_holder<void(error_code)> waiters_;
};

}


#if !defined(BOOST_ASEM_HEADER_ONLY)
extern template
struct basic_barrier<mt, net::any_io_executor >;
#endif

BOOST_ASEM_END_NAMESPACE


#if defined(BOOST_ASEM_HEADER_ONLY)
#include <boost/asem/mt/impl/basic_barrier.ipp>
#endif

#endif //BOOST_ASEM_MT_BASIC_BARRIER_HPP
