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

#ifdef flagGCC_LIKE
#include <cxxabi.h>

struct Free
{
    void operator()(void* v)
    {
        free(v);
    }
};

using CharPtr = std::unique_ptr<char, Free>;

std::string demangle(const char* name)
{
    int status;
    CharPtr demangled{abi::__cxa_demangle(name, 0, 0, &status)};
    VERIFY(status == 0, "Invalid demangle status");
    return demangled.get();
}

#else

std::string demangle(const char* name)
{
    return name;
}

#endif

namespace synca {

struct StatRegistrar
{
    void registerValue(const char* name, StatCounter* v);
    void dump();

private:
    using Lock = std::unique_lock<std::mutex>;
    mutable std::mutex mutex_;
    std::map<std::string, StatCounter*> infos_;
};

void post(Handler h)
{
    ((IScheduler&)scheduler<DefaultTag>()).schedule(std::move(h));
}

void detail::registerStat(const char *name, StatCounter *v)
{
    // TODO: think about post function for default scheduler
    post([name, v] {
        try
        {
            single<StatRegistrar>().registerValue(name, v);
        }
        catch (std::exception& e)
        {
            RLOG("Exception on stat registering: " << e.what());
        }
    });
}

void replaceAll(std::string& s, const std::string& rm, const std::string& ad = "")
{
    //LOG("from: " << s << ", rm: " << rm);
    while (true)
    {
        size_t p = s.find(rm);
        if (p == std::string::npos)
            return;
        s.replace(p, rm.size(), ad);
        //LOG("to: " << s << ", p: " << p);
    }
}

std::string remove(const std::string& s, std::initializer_list<const char*> lst)
{
    std::string r = s;
    for (auto&& l: lst)
        replaceAll(r, l);
    return r;
}

void StatRegistrar::registerValue(const char *name, synca::StatCounter *v)
{
    std::string reducedName = remove(demangle(name), {"struct ", "synca::", "InstanceStat", "Stat", "<", ">"});
    Lock _{mutex_};
    VERIFY(infos_.find(reducedName) == infos_.end(), "Statistics name must be unique");
    infos_[reducedName] = v;
}

void StatRegistrar::dump()
{
    Lock _{mutex_};
    RLOG("dumping statistics");
    for (auto&& v: infos_)
    {
        RLOG("--- stat: " << v.first << ": " << v.second->load(std::memory_order_relaxed));
    }
}

void dumpStats()
{
    single<StatRegistrar>().dump();
}

}
