/*
 * Copyright 2015 Grigory Demchenko (aka gridem)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <utility>

#include <boost/preprocessor/tuple/to_list.hpp>
#include <boost/preprocessor/list/for_each.hpp>
//#include <boost/preprocessor/punctuation/remove_parens.hpp>

// TODO: fix the msvc compiler
#define BOOST_PP_REMOVE_PARENS(D) D

#define FWD_CTOR(D_type, D_base) \
    template<typename... V> D_type(V&&... v) : D_base{std::forward<V>(v)...} {}

#define FWD_CTOR_TBASE(D_type) FWD_CTOR(D_type, T_base)
#define FWD_CTOR_ADAPTER() FWD_CTOR_TBASE(Adapter)

// decltype(v) instead of V is a workaround for VS compiler
#define DECL_FN_ADAPTER(D_name) \
    template<typename... V> \
    auto D_name(V&&... v) \
    { \
        return this->call([](auto& t, auto&&... x) { \
            return t.D_name(std::forward<decltype(x)>(x)...); \
        }, std::forward<V>(v)...); \
    }

#define DECL_FN_ADAPTER_ITERATION(D_r, D_data, D_elem)      DECL_FN_ADAPTER(D_elem)

#define DECL_FN_NAMES(...) \
    FWD_CTOR_ADAPTER() \
    BOOST_PP_LIST_FOR_EACH(DECL_FN_ADAPTER_ITERATION, , BOOST_PP_TUPLE_TO_LIST((__VA_ARGS__)))

#define DECL_ADAPTER(D_type, ...) \
    template<typename T_base> \
    struct Adapter<BOOST_PP_REMOVE_PARENS(D_type), T_base> : T_base \
    { \
        DECL_FN_NAMES(__VA_ARGS__) \
    };

template<typename T, typename T_base>
struct Adapter : T_base
{
    DECL_FN_NAMES(read, write, put, get, on, vote, commit)
};

template<typename T_base, typename T_locker>
struct BaseLocker : T_base, private T_locker
{
    FWD_CTOR_TBASE(BaseLocker)
protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        struct Lock
        {
            explicit Lock(BaseLocker& l) : l_{l}    { l_.T_locker::lock(); }
            ~Lock()                                 { l_.T_locker::unlock(); }
        private:
            BaseLocker& l_;
        };

        Lock _{*this};
        return T_base::call(f, std::forward<V>(v)...);
    }
};

template<typename T_base>
struct BaseValue : T_base
{
    FWD_CTOR_TBASE(BaseValue)
protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        return f(*this, std::forward<V>(v)...);
    }
};

template<typename T>
struct BaseRef
{
    BaseRef(T& t) : ref_{t} {}
protected:
    template<typename F, typename... V>
    auto call(F f, V&&... v)
    {
        return f(ref_, std::forward<V>(v)...);
    }
private:
    T& ref_;
};
