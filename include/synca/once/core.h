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

// TODO: remove logs from headers

namespace synca {

struct ICancel : IObject
{
    virtual void cancel() noexcept = 0;
    virtual void cleanup() noexcept {}
};

typedef std::function<void(Handler)> ProceedHandler;

int index();
Goer go(Handler handler, IScheduler& scheduler);
Goer go(Handler handler);
void goN(int n, Handler handler);

void teleport(IScheduler& scheduler);
void handleEvents();
bool disableEvents();
void disableEventsAndCheck();
void enableEvents();
void enableEventsAndCheck();
void waitForAll();
void deferProceed(ProceedHandler proceed);
void waitForDone();
void goWait(std::initializer_list<Handler> handlers);
void goWait2(std::initializer_list<Handler> handlers);
void reschedule();

struct EventsGuard
{
    EventsGuard();
    ~EventsGuard();
private:
    bool wasEnabled;
};

struct DtorEventsGuard
{
    DtorEventsGuard();
    ~DtorEventsGuard() noexcept;
private:
    bool wasEnabled;
};

/*
struct Waiter
{
    Waiter();
    ~Waiter();

    Waiter& go(Handler h);
    void wait();

private:
    void init0();

    Handler proceed;
    std::shared_ptr<Waiter> proceeder;
};
*/

struct AsyncWaiter : Doer
{
    void inc()
    {
        counter.fetch_add(1, std::memory_order_relaxed);
    }

    void dec()
    {
        if (counter.fetch_sub(1, std::memory_order_relaxed) == 1)
            done();
    }

    void wait()
    {
        if (counter.fetch_sub(1, std::memory_order_relaxed) != 1)
            waitForDone();
    }

    bool isDone() const
    {
        return counter.load(std::memory_order_relaxed) == 0;
    }

private:
    Atomic<int> counter {1};
};

struct Completer
{
    void add(int val);
    void inc();
    void dec(); // only dec invokes done
    bool isDone() const; // TODO: don't need to be used, consider removing

private:
    Atomic<int> counter_;
    Doer doer_;
};

// TODO: add detached awaiter using DetachedDone
struct Awaiter : ICancel
{
    ~Awaiter();
    Awaiter& go(Handler h);
    void wait();
    void cancel() noexcept override;
    bool empty() const;

private:
    void cleanup() noexcept override;

    Completer completer_;
    std::vector<Goer> goers_;
};

struct Completer2
{
    Completer2();

    void start() noexcept;
    void done() noexcept; // threadsafe
    void wait();

private:
    bool add(int v) noexcept;
    void wait0();

    int jobs_ = 0;
    Atomic<int> counter_;
    Doer doer_;
};

// TODO: rename awaiter to group
struct Awaiter2
{
    ~Awaiter2() noexcept;
    Awaiter2& go(Handler h);
    void wait();
    void cancel() noexcept;
    bool empty() const noexcept;

private:
    Completer2 completer_;
    std::vector<Goer> goers_;
};

template<typename T_doer>
struct DoerScoped
{
    ~DoerScoped()
    {
        doer_.done();
    }

    T_doer doer_;
};

struct DetachableAwaiter
{
    ~DetachableAwaiter() noexcept;

    DetachableAwaiter& go(Handler handler);
    void wait();

private:
    using SharedDoer = std::shared_ptr<DoerScoped<DetachableDoer>>;

    SharedDoer sharedDoer();

    SharedDoer sharedDoer_;
};

size_t goAnyWait(std::initializer_list<Handler> handlers);

template<typename T_result>
boost::optional<T_result> goAnyResult(std::initializer_list<std::function<boost::optional<T_result>()>> handlers)
{
    typedef boost::optional<T_result> Result;
    typedef std::function<void(Result&&)> ResultHandler;

    struct Counter
    {
        Counter(ResultHandler proceed_) : proceed(std::move(proceed_)) {}
        ~Counter()
        {
            tryProceed(Result());
        }

        void tryProceed(Result&& result)
        {
            if (++ counter == 1)
                proceed(std::move(result));
        }

    private:
        Atomic<int> counter;
        ResultHandler proceed;
    };

    Result result;
    deferProceed([&handlers, &result](Handler proceed) {
        std::shared_ptr<Counter> counter = std::make_shared<Counter>(
        [&result, proceed](Result&& res) {
            result = std::move(res);
            proceed();
        });
        for (const auto& handler: handlers)
        {
            go([counter, &handler] {
                Result result = handler();
                if (result)
                    counter->tryProceed(std::move(result));
            });
        }
    });
    return result;
}

// TODO: consider using waitForAll in dtor
struct Alone : IScheduler
{
    explicit Alone(IService& service, const char* name = "alone");
    ~Alone();

    void schedule(Handler handler);
    const char* name() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    const char* strandName;
};

struct TimeoutTag;
struct Timeout
{
    Timeout(int ms);
    ~Timeout();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

void sleepa(int ms);

struct Timer
{
    Timer();
    ~Timer();

    void reset(int ms, Handler handler);
    void reset();

protected:
    void detach();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct GlobalTimer : Timer, WithCleanup
{
private:
    void cleanup() override;
};

struct Service
{
    void attach(IService&);
    void detach();

    operator AsioService&() const;

private:
    AsioService* service {nullptr};
};

template<typename T_tag>
Service& service()
{
    return single<Service, T_tag>();
}

struct Scheduler
{
    void attach(IScheduler& s);
    void detach();

    operator IScheduler&() const;

private:
    IScheduler* scheduler {nullptr};
};

struct DefaultTag;

template<typename T_tag>
Scheduler& scheduler()
{
    return single<Scheduler, T_tag>();
}

}
