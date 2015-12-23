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

#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>

#ifdef flagMSC
#   define TLS                      __declspec(thread)
#else
#   define TLS                      __thread
#endif

#define WAIT_FOR(D_condition)       while (!(D_condition)) sleepFor(100)
#define VERIFY(D_cond, D_str)       if (!(D_cond)) raiseException("Verification failed: " #D_cond ": " D_str)

using Byte = unsigned char;
using Ptr = Byte*;
using PtrArray = Ptr*;
using IntArray = int*;
using Buffer = std::vector<Byte>;
using Handler = std::function<void ()>; // TODO: consider renaming to Action
using Predicate = std::function<bool ()>; // TODO: consider renaming to Action

void sleepFor(int ms);
void raiseException(const char* msg);
void threadYield();
int threadConcurrency();
void logException(std::exception& e);
void waitFor(Predicate condition);
std::string currentTimeDate();

struct IObject
{
    virtual ~IObject() {}
};

template<typename T, typename T_tag = T>
T& single()
{
    static T t;
    return t;
}

template<typename T, typename T_tag = T>
const T& singleConst()
{
    return single<T, T_tag>();
}

template<typename T>
struct Atomic : std::atomic<T>
{
    Atomic(T v = 0) : std::atomic<T>(v) {}
};

template<typename T>
std::atomic<int>& atomic()
{
    return single<Atomic<int>, T>();
}

template<typename T, typename T_tag = T>
T*& tlsPtr()
{
    static TLS T* t = nullptr;
    return t;
}

template<typename T, typename T_tag = T>
T& tls()
{
    T* t = tlsPtr<T, T_tag>();
    VERIFY(t != nullptr, "TLS must be initialized");
    return *t;
}

/*
 * Class to avoid unnecessary copying for guards.
 * =delete for copy ctor results to the compilation error.
 * So you should rely on RVO optimization here.
 * To verify RVO the exception will be thrown.
 */
struct DisableCopy
{
    DisableCopy() = default;

    DisableCopy(const DisableCopy&)
    {
        forbidden();
    }

    DisableCopy& operator=(const DisableCopy&) = delete;

private:
    void forbidden()
    {
        raiseException("Forbidden: looks like RVO optimization goes away");
    }
};

template<typename T>
struct TlsGuard : DisableCopy
{
    TlsGuard(T* this_)
    {
        auto& p = tlsPtr<T>();
        that = p;
        p = this_;
    }

    ~TlsGuard()
    {
        tlsPtr<T>() = that;
    }

private:
    T* that;
};

// here I rely on RVO optimization to avoid unnecesary checkings and implementation complexity
template<typename T>
TlsGuard<T> tlsGuard(T* this_)
{
    return this_;
}

template<typename T>
struct Shared : std::shared_ptr<T>
{
    Shared() : std::shared_ptr<T> (std::make_shared<T>()) {}
    //Shared(const Shared&) = default;
    //Shared(Shared&&) = default;
};

template<typename T>
struct Queue
{
    bool empty() const
    {
        return q_.empty();
    }

    T pop()
    {
        T t{std::move(q_.front())};
        q_.pop();
        return t;
    }

    void push(T t)
    {
        q_.emplace(std::move(t));
    }

    void swap(Queue& q)
    {
        q_.swap(q.q_);
    }

private:
    std::queue<T> q_;
};

