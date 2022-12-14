// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_ASEM_ST_IMPL_BASIC_MUTEX_IPP
#define BOOST_ASEM_ST_IMPL_BASIC_MUTEX_IPP

#include <boost/asem/detail/config.hpp>
#include <boost/asem/detail/basic_op.hpp>
#include <boost/asem/st/basic_mutex.hpp>

BOOST_ASEM_BEGIN_NAMESPACE
namespace detail
{

void mutex_impl<st>::unlock()
{
    // release a pending operations
    if (waiters_.next_ == &waiters_)
    {
        locked_ = false;
        return;
    }
    static_cast< detail::wait_op * >(waiters_.next_)->complete(std::error_code());
}

bool mutex_impl<st>::try_lock()
{
    if (locked_)
        return false;
    else
        return locked_ = true;
}

void
mutex_impl<st>::add_waiter(detail::wait_op *waiter) noexcept
{
    waiter->link_before(&waiters_);
}

}
BOOST_ASEM_END_NAMESPACE


#endif //BOOST_ASEM_ST_IMPL_BASIC_MUTEX_IPP
