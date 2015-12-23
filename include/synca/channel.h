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

#include <queue>
#include <mutex>

#include "synca.h"

// TODO: refactor (remove mutex, move to once folder?) [solution: implement pimpl std mutex]

namespace synca {

// with iterators
template<typename T>
struct Channel
{
public:
    struct Iterator
    {
        Iterator() = default;
        Iterator(Channel& c) : ch(&c)            { ++*this; }

        T& operator*()                           { return val; }
        Iterator& operator++()                   { if (!ch->get(val)) ch = nullptr; return *this; }
        bool operator!=(const Iterator& i) const { return ch != i.ch; }
    private:
        T val;
        Channel* ch = nullptr;
    };
    
    Iterator begin()                             { return {*this}; }
    static Iterator end()                        { return {}; }
    
    void put(T val)
    {
        Lock lock{mutex};
        VERIFY(!closed, "Channel was closed");
        while (!waiters_.empty())
        {
            Waiter w = waiters_.pop();
            if (w.acquire())
            {
                lock.unlock();
                w.set(std::move(val));
                return;
            }
        }
        queue.push(std::move(val));
    }
    
    bool get(T& val)
    {
        Lock lock{mutex};
        if (!queue.empty())
        {
            val = queue.pop();
            return true;
        }
        if (closed)
            return false;
        bool wasSet = false;
        waiters_.push(Waiter{val, wasSet});
        lock.unlock();
        waitForDone();
        return wasSet;
    }
    
    bool empty() const
    {
        Lock lock{mutex};
        return queue.empty();
    }
    
    T get()
    {
        T val;
        get(val);
        return val;
    }
    
    void open()
    {
        Lock lock{mutex};
        closed = false;
    }
    
    void close()
    {
        Lock lock{mutex};
        if (closed)
            return;
        closed = true;
        Queue<Waiter> ws;
        waiters_.swap(ws);
        lock.unlock();
        while (!ws.empty())
            ws.pop().reset();
    }
    
private:
    using Lock = std::unique_lock<std::mutex>;

    struct Waiter
    {
        explicit Waiter(T& t, bool& wasSet) : t_{&t}, wasSet_{&wasSet} {}

        bool acquire()
        {
            return doer_.acquire();
        }

        void set(T t) noexcept
        {
            *t_ = std::move(t); // move must be noexcept
            *wasSet_ = true;
            doer_.releaseAndDone();
        }

        void reset() noexcept
        {
            doer_.done();
        }

    private:
        T* t_;
        bool* wasSet_;
        DetachableDoer doer_;
    };

    //Waiters waiters;
    mutable std::mutex mutex;
    Queue<T> queue;
    Queue<Waiter> waiters_;
    bool closed = false;
};

}
