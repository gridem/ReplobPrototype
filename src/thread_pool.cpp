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

#include "synca_impl.h"

// ThreadPool log: inside ThreadPool functionality
#define PLOG(D_msg)             TLOG("@" << this->name() << ": " << D_msg)

namespace synca {

TLS int t_number = 0;
TLS const char* t_name = "main";

const char* name()
{
    return t_name;
}

int number()
{
    return t_number;
}

std::thread createThread(Handler handler, int number, const char* name)
{
    return std::thread([handler, number, name] {
        t_number = number + 1;
        t_name = name;
        try
        {
            TLOG("thread created");
            handler();
            TLOG("thread ended");
        }
        catch (std::exception& e)
        {
            (void) e;
            TLOG("thread ended with error: " << e.what());
        }
    });
}

struct ThreadPool::Impl
{
    using Work = boost::asio::io_service::work;

    Impl(size_t threadCount, const char* name) : tpName(name)
    {
        resetWork();
        threads.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++ i)
            threads.emplace_back(createThread([this] { loop(); }, i, name));
        PLOG("thread pool created with threads: " << threadCount);
    }

    ~Impl()
    {
        mutex.lock();
        toStop = true;
        work.reset();
        mutex.unlock();
        PLOG("stopping thread pool");
        for (size_t i = 0; i < threads.size(); ++ i)
            threads[i].join();
        PLOG("thread pool stopped");
    }

    void resetWork()
    {
        work.reset(new Work(service));
    }

    const char* name() const
    {
        return tpName;
    }

    void loop()
    {
        while (true)
        {
            service.run();
            std::unique_lock<std::mutex> lock(mutex);
            if (toStop)
                break;
            if (!work)
            {
                resetWork();
                service.reset();
                lock.unlock();
                cond.notify_all();
            }
        }
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        work.reset();
        while (true)
        {
            cond.wait(lock);
            TLOG("WAIT: waitCompleted: " << (work != nullptr));
            if (work)
                break;
        }
    }

    const char* tpName;
    std::unique_ptr<Work> work;
    AsioService service;
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::condition_variable cond;
    bool toStop = false;
};

ThreadPool::ThreadPool(size_t threadCount, const char* name)
    : impl(new Impl(threadCount, name))
{
}

ThreadPool::~ThreadPool()
{
}

void ThreadPool::schedule(Handler handler)
{
    impl->service.post(std::move(handler));
}

void ThreadPool::wait()
{
    impl->wait();
}

const char* ThreadPool::name() const
{
    return impl->name();
}

AsioService& ThreadPool::asioService()
{
    return impl->service;
}

}
