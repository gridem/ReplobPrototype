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

#include <boost/date_time/posix_time/posix_time.hpp>

#include "synca_impl.h"

void sleepFor(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void raiseException(const char* msg)
{
    throw std::runtime_error(msg);
}

void threadYield()
{
    std::this_thread::yield();
}

int threadConcurrency()
{
    return std::thread::hardware_concurrency();
}

void logException(std::exception& e)
{
    RJLOG("Error: " << e.what());
}

void waitFor(Predicate condition)
{
    int interval = 10;
    for (int i = 0; !condition(); ++ i)
    {
        if (i == interval)
        {
            interval *= 3;
            synca::dumpStats();
        }
        else
        {
            sleepFor(100);
        }
    }
    synca::dumpStats();
}

std::string currentTimeDate()
{
    return boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time());
}
