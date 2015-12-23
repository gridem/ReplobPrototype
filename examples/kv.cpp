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

#include "server.h"

using boost::optional;
using std::unordered_map;
using std::string;

struct KV
{
    optional<string> get(const string& key) const;
    void set(const string& key, const optional<string>& value);

    template<typename F>
    void forEach(F f) const
    {
        for (auto&& v: kv_)
            f(v);
    }

private:
    unordered_map<string, string> kv_;
};

optional<string> KV::get(const string& key) const
{
    if (kv_.count(key) == 0)
        return {};
    return kv_.at(key);
}

void KV::set(const string& key, const optional<string>& value)
{
    if (value)
        kv_[key] = *value;
    else
        kv_.erase(key);
}

DECL_ADAPTER(KV, get, set)

void kv()
{
    replob<KV>().set(string{"hello"}, string{"world!"});

    auto world = replob<KV>().get(string{"hello"});
    RJLOG("world: " << *world);

    auto localWorld = replobLocal<KV>().get(string{"hello"});
    RJLOG("localWorld: " << *localWorld);

    {
        auto world = replobLocal<KV>().get(string{"hello"}).value_or("world!");
        replob<KV>().set(string{"hello"}, "hello " + world);
    }
    {
        auto localWorld = replobLocal<KV>().get(string{"hello"});
        RJLOG("localWorld: " << *localWorld);
    }
    MReplobTransactInstance(KV) {
        auto world = $.get(string{"hello"}).value_or("world!");
        $.set(string{"hello"}, "hello " + world);
    };
    {
        auto localWorld = replobLocal<KV>().get(string{"hello"});
        RJLOG("localWorld: " << *localWorld);
    }
    auto valueLength = MReplobTransactLocalInstance(KV) {
        return $.get(string{"hello"}).value_or("").size();
    };
    RJLOG("value length: " << valueLength);
    {
        auto valueLength = MReplobTransactInstance(KV) {
            auto world = $.get(string{"hello"});
            $.set(string{"another"}, world);
            return world.value_or("").size();
        };
        RJLOG("value length: " << valueLength);
    }
    auto valuesSize = MReplobTransactLocalInstance(KV) {
        size_t sz = 0;
        $.forEach([&sz](auto&& v) {
            sz += v.second.size();
        });
        return sz;
    };
    RJLOG("values size: " << valuesSize);
}

void starter()
{
    if (thisNode() == 1)
        go(kv);
    sleepFor(1 * 1000);
}

int main(int argc, const char* argv[])
{
    try
    {
        Server::serve(starter);
    }
    catch (std::exception& e)
    {
        RLOG("Error: " << e.what());
        return 1;
    }
    return 0;
}
