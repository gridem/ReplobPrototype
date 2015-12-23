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

namespace synca {

// TODO: move it somewhere?
const char* name();
int number();

struct IScheduler : IObject
{
    virtual void schedule(Handler handler) = 0;
    virtual const char* name() const { return "<unknown>"; }
};

struct AsioService;

struct IService : IObject
{
    virtual AsioService& asioService() = 0;
};

struct ThreadPool : IScheduler, IService
{
    ThreadPool(size_t threadCount, const char* name = "");
    ~ThreadPool();

    void schedule(Handler handler) override;
    void wait();
    const char* name() const;

private:
    AsioService& asioService() override;

    struct Impl;
    std::unique_ptr<Impl> impl;
};

}
