/*
 *  Copyright (c) 2016, Indium Games
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 *
 */
 
#ifndef __STD_ADDITIONS_H
#define __STD_ADDITIONS_H

#include <type_traits>
#include <functional>

/*
 * This file implements invoke functions which are similar to be implemented in
 * C++17, but uses Tuple instead of variadic template.
 */

namespace detail {
    template <typename F, typename Tuple, bool Done, int Total, int... N>
    struct call_impl {
        static auto call(F &&f, Tuple &&t) {
            return call_impl<F, Tuple, Total == 1 + sizeof...(N), Total,
                        N..., sizeof...(N)>::call(f, std::forward<Tuple>(t));
        }
    };

    template <typename F, typename Tuple, int Total, int... N>
    struct call_impl<F, Tuple, true, Total, N...> {
        static auto call(F &&f, Tuple &&t) {
            return std::forward<F>(f)(std::get<N>(std::forward<Tuple>(t))...);
        }
    };
    
    
    template <typename F, typename Tuple>
    struct empty_call_impl {
        static void call(F &&f, Tuple &&t) {}
    };
    
    
    template <typename Tuple, bool Done, int Total, int... N>
    struct call_pmf_impl {
        template<typename Ret, typename T, typename Obj>
        static auto call(Ret T::*pmf, Obj &&objectPtr, Tuple &&t) {
            return call_pmf_impl
                <Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>
                ::call(pmf, objectPtr, std::forward<Tuple>(t));
        }
    };

    template <typename Tuple, int Total, int... N>
    struct call_pmf_impl<Tuple, true, Total, N...> {
        template<typename Ret, typename T, typename Obj>
        static auto call(Ret T::*pmf, Obj &&objectPtr, Tuple &&t) {
            return ((*std::forward<Obj>(objectPtr)).*pmf)
                                    (std::get<N>(std::forward<Tuple>(t))...);
        }
    };
    template<typename Tuple>
    struct empty_call_pmf_impl {
        template<typename Ret, typename T, typename Obj>
        static void call(Ret T::*pmf, Obj &&objectPtr, Tuple &&t) {}
    };
}

// To invoke C function with arguments from tuple.
template <class F, class Tuple>
auto INVOKE(F &&f, Tuple &&t) {
    typedef typename std::decay<Tuple>::type ttype;
    return detail::call_impl<F, Tuple, 0 == std::tuple_size<ttype>::value,
                std::tuple_size<ttype>::value>::call(f, std::forward<Tuple>(t));
}


// To invoke C++ member function with arguments from tuple.
template<typename Ret, typename T, typename Obj, typename Tuple>
auto INVOKE(Ret T::*pmf, Obj &&objectPtr, Tuple &&t) {
    typedef typename std::decay<Tuple>::type ttype;
    return detail::call_pmf_impl<Tuple, 0 == std::tuple_size<ttype>::value,
                                std::tuple_size<ttype>::value>
                                ::call(pmf, objectPtr, std::forward<Tuple>(t));
}


/*
 * A compile time helper for variadic templates.
 */
template<class F>
struct function_traits;
 
// function pointer
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)> {};

template<class R, class... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
 
    static constexpr std::size_t arity = sizeof...(Args);
 
    template <std::size_t N>
    struct argument {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    };
};

#endif
