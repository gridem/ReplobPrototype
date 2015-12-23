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

/*
 * TODO: add proceed interface with normal, sync, forced sync options
 */

namespace synca
{

Goer go(Handler handler, IScheduler &scheduler)
{
    return Journey::start(std::move(handler), scheduler);
}

Goer go(Handler handler)
{
    return Journey::start(std::move(handler), scheduler<DefaultTag>());
}

void goN(int n, Handler h)
{
    go(n == 1 ? h : [n, h] {
        for (int i = 0; i < n; ++i)
            go(h);
    });
}

void teleport(IScheduler &scheduler)
{
    journey().teleport(scheduler);
}

void handleEvents()
{
    journey().handleEvents();
}

bool disableEvents()
{
    return journey().disableEvents();
}

void disableEventsAndCheck()
{
    journey().enableEventsAndCheck();
}

void enableEvents()
{
    journey().enableEvents();
}

void enableEventsAndCheck()
{
    journey().enableEventsAndCheck();
}

void deferProceed(ProceedHandler proceed)
{
    journey().deferProceed(proceed);
}

void waitForDone()
{
    journey().waitForDone();
}

void goWait(std::initializer_list<Handler> handlers)
{
    deferProceed([&handlers](Handler proceed) {
        std::shared_ptr<void> proceeder(nullptr, [proceed](void *) { proceed(); });
        for (const auto &handler : handlers) {
            go([proceeder, &handler] { handler(); });
        }
    });
}

void goWait2(std::initializer_list<Handler> handlers)
{
    Awaiter2 a;
    for (const auto &handler : handlers) {
        a.go(handler);
    }
    a.wait();
}

EventsGuard::EventsGuard()
    : wasEnabled(disableEvents())
{
}

EventsGuard::~EventsGuard()
{
    if (wasEnabled)
        enableEventsAndCheck();
}

DtorEventsGuard::DtorEventsGuard()
    : wasEnabled(disableEvents())
{
    JLOG("dtor guard created");
}

DtorEventsGuard::~DtorEventsGuard() noexcept
{
    JLOG("dtor guard destroyed: was " << wasEnabled);
    if (wasEnabled)
        enableEvents();
}

/*
Waiter::Waiter()
{
    proceed = journey().proceedHandler();
    init0();
}

Waiter::~Waiter()
{
    proceed = nullptr; // to avoid unnecessary proceeding
}

Waiter &Waiter::go(Handler handler)
{
    auto &holder = proceeder;
    synca::go([holder, handler] { handler(); });
    return *this;
}

void Waiter::wait()
{
    if (proceeder.unique()) {
        JLOG("everything done, nothing to do");
        return;
    }
    defer([this] { auto toDestroy = std::move(proceeder); });
    init0();
}

void Waiter::init0()
{
    proceeder.reset(this, [](Waiter *w) {
        if (w->proceed != nullptr) {
            TLOG("wait completed, proceeding");
            w->proceed();
        }
    });
}
*/

size_t goAnyWait(std::initializer_list<Handler> handlers)
{
    VERIFY(handlers.size() >= 1, "Handlers amount must be positive");

    size_t index = static_cast<size_t>(-1);
    deferProceed([&handlers, &index](Handler proceed) {
        std::shared_ptr<Atomic<int> > counter = std::make_shared<Atomic<int> >();
        size_t i = 0;
        for (const auto &handler : handlers) {
            go([counter, proceed, &handler, i, &index] {
                handler();
                if (++*counter == 1) {
                    index = i;
                    proceed();
                }
            });
            ++i;
        }
    });
    VERIFY(index < handlers.size(), "Incorrect index returned");
    return index;
}

struct Alone::Impl : boost::asio::io_service::strand
{
    using boost::asio::io_service::strand::strand;
};

Alone::Alone(IService &service, const char *name)
    : impl{ new Impl{ service.asioService() } }, strandName(name)
{
}

Alone::~Alone()
{
}

void Alone::schedule(Handler handler)
{
    impl->post(std::move(handler));
}

const char *Alone::name() const
{
    return strandName;
}

struct Timeout::Impl : boost::asio::deadline_timer
{
    using boost::asio::deadline_timer::deadline_timer;
};

Timeout::Timeout(int ms)
    : impl{ new Impl(service<TimeoutTag>(), boost::posix_time::milliseconds(ms)) }
{
    Goer goer = journey().goer();
    impl->async_wait([goer](const ErrorCode& error) mutable {
        if (!error)
            goer.timedout();
    });
}

Timeout::~Timeout()
{
    impl->cancel_one();
    //handleEvents(); // don't needed due to forced implementation to wake up
}

void sleepa(int ms)
{
    boost::asio::deadline_timer timer{service<TimeoutTag>(), boost::posix_time::milliseconds(ms)};
    DetachableDoer d;
    timer.async_wait([d](const ErrorCode& error) mutable {
        if (!error)
            d.done();
    });
    waitForDone();
}

void Service::attach(IService& s)
{
    service = &s.asioService();
}

void Service::detach()
{
    service = nullptr;
}

Service::operator AsioService&() const
{
    VERIFY(service != nullptr, "Service is not attached");
    return *service;
}

void Scheduler::attach(IScheduler& s)
{
    scheduler = &s;
}

void Scheduler::detach()
{
    scheduler = nullptr;
}

Scheduler::operator IScheduler&() const
{
    VERIFY(scheduler != nullptr, "Scheduler is not attached");
    return *scheduler;
}

template<typename F>
struct Finally
{
    Finally(F f) : f_{std::move(f)} {}
    ~Finally()
    {
        //JLOG("try catch finally: finally");
        try
        {
            f_();
        }
        catch (...)
        {
            JLOG("ooopsss!!!!");
        }
    }
private:
    F f_;
};

template<typename F_try, typename F_catch, typename F_finally>
void tryCatchFinally(F_try fTry, F_catch fCatch, F_finally fFinally)
{
    Finally<F_finally> f{std::move(fFinally)};
    try
    {
        fTry();
    }
    catch(...)
    {
        fCatch();
        throw;
    }
}

template<typename F_try, typename F_finally>
void tryFinally(F_try fTry, F_finally fFinally)
{
    Finally<F_finally> f{std::move(fFinally)};
    fTry();
}

template<typename F>
struct Finally2
{
    Finally2(F&& f) : f_{std::forward<F>(f)} {}
    ~Finally2() noexcept { f_(); }
private:
    F&& f_;
};

template<typename F_try, typename F_finally>
void tryFinally2(F_try&& fTry, F_finally&& fFinally)
{
    Finally2<F_finally> f{std::forward<F_finally>(fFinally)};
    fTry();
}

void Completer::add(int val)
{
    int old = counter_.fetch_add(val, std::memory_order_relaxed);
    if (old + val == 0)
        doer_.done();
}

void Completer::inc()
{
    add(1);
}

void Completer::dec()
{
    add(-1);
}

bool Completer::isDone() const
{
    return counter_.load(std::memory_order_relaxed) == 0;
}

Awaiter::~Awaiter()
{
    if (empty())
        return;
    completer_.add(goers_.size());
    journey().waitForDoneAlwaysGuarded(*this);
}

Awaiter& Awaiter::go(Handler h)
{
    goers_.emplace_back(synca::go([h, this] {
        tryFinally2(h, [&]{completer_.dec();});
    }));
    return *this;
}

void Awaiter::wait()
{
    // shortcut optimization
    if (empty())
        return;
    completer_.add(goers_.size());
    // TODO: consider wait in dtor on events
    journey().waitForDoneAlways(*this);
}

void Awaiter::cancel() noexcept
{
    for (auto&& g: goers_)
        g.cancel();
}

bool Awaiter::empty() const
{
    return goers_.empty();
}

void Awaiter::cleanup() noexcept
{
    goers_.clear();
}

struct CompleterScope
{
    CompleterScope(Completer2& c) : c_{c} {}
    ~CompleterScope() noexcept {c_.done();}
private:
    Completer2& c_;
};

Completer2::Completer2()
    : doer_{journey().doer()}
{
}

void Completer2::start() noexcept
{
    -- jobs_;
}

void Completer2::done() noexcept
{
    JLOG("completer: done: " << jobs_);
    if (add(-1))
        doer_.done();
}

void Completer2::wait()
{
    if (jobs_ == 0)
        return;
    wait0();
    // TODO: VERIFY counter
    jobs_ = 0;
}

bool Completer2::add(int v) noexcept
{
    return counter_.fetch_add(v, std::memory_order_relaxed) + v == 0;
}

void Completer2::wait0()
{
    if (jobs_ < 0)
    {
        jobs_ = -jobs_;
        if (add(jobs_))
        {
            JLOG("completer: done jobs: " << jobs_);
            return;
        }
        JLOG("completer: first wait jobs: " << jobs_);
    }
    else
    {
        if (counter_.load(std::memory_order_relaxed) == 0)
        {
            JLOG("completer: done on second wait: " << jobs_);
            return;
        }
    }
    JLOG("completer: wait jobs: " << jobs_);
    waitForDone();
    JLOG("completer: done");
}

Awaiter2::~Awaiter2() noexcept
{
    if (empty())
        return;
    DtorEventsGuard _;
    cancel();
    wait();
}

Awaiter2& Awaiter2::go(Handler h)
{
    // FIXME: what if goers_ throws an exception?
    goers_.emplace_back(synca::go([h, this] {
        CompleterScope _{completer_};
        h();
    }));
    completer_.start();
    return *this;
}

void Awaiter2::wait()
{
    completer_.wait();
    goers_.clear();
}

void Awaiter2::cancel() noexcept
{
    for (auto&& g: goers_)
        g.cancel();
}

bool Awaiter2::empty() const noexcept
{
    return goers_.empty();
}

DetachableAwaiter::~DetachableAwaiter() noexcept
{
    if (!sharedDoer_)
        return;
    sharedDoer_.reset();
    journey().detachDoers();
}

DetachableAwaiter &DetachableAwaiter::go(Handler h)
{
    auto d = sharedDoer();
    synca::go([h, d] {h();});
    return *this;
}

void DetachableAwaiter::wait()
{
    if (!sharedDoer_)
        return;
    sharedDoer_.reset();
    waitForDone();
}

DetachableAwaiter::SharedDoer DetachableAwaiter::sharedDoer()
{
    if (!sharedDoer_)
        sharedDoer_ = std::make_shared<DoerScoped<DetachableDoer>>();
    return sharedDoer_;
}

void reschedule()
{
    journey().reschedule();
}

struct Timer::Impl : boost::asio::deadline_timer
{
    using boost::asio::deadline_timer::deadline_timer;
};

// TODO: introduce concept of main threadpoll with all registered services and default scheduler
Timer::Timer()
    : impl_{new Impl{service<TimeoutTag>()}}
{
}

Timer::~Timer()
{
    if (impl_)
        reset();
}

void Timer::reset(int ms, Handler handler)
{
    impl_->expires_from_now(boost::posix_time::milliseconds(ms));
    impl_->async_wait([handler](const ErrorCode& error) {
        if (!error)
            go(std::move(handler));
    });
}

void Timer::reset()
{
    impl_->cancel_one();
}

void Timer::detach()
{
    impl_.reset();
}

void GlobalTimer::cleanup()
{
    detach();
}

}
