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

//#define JJLOG       JLOG
#define JJLOG       NLOG

namespace synca {

TLS int t_cancellation = 0;

int index()
{
    Journey* j = tlsPtr<Journey>();
    return j ? j->index() : 0;
}

struct JourneyStat
{
    struct SendRaiseEvent {};
    struct RaiseCancelEvent {};
    struct RaiseTimeoutEvent {};
    struct DoneWithoutRescheduling {};
    struct ProceedOnExit {};
    struct ProceedOnDone {};
    struct ProceedOnRaiseEvent {};
    struct ProceedOnReleaseDone {};
    struct ForceDoneByCancel {};
    struct ForceDoneByCancelGuarded {};
    struct AcquiredOnDisabledEvents {};
    struct AcquiredFailed {};
    struct Acquired {};
    struct ReleaseOnDisabledEvents {};
    struct Suspended {};
    struct CoroYield {};
};

JourneyState::JourneyState(synca::Journey &j)
    : j_{j}, index_{j.index()}
{
}

void JourneyState::cancel()
{
    addRaiseEvent(FlagCancelled);
}

void JourneyState::timedout()
{
    addRaiseEvent(FlagTimedout);
}

bool JourneyState::disableEvents()
{
    return (resetFlags0(FlagEventsEnabled) & FlagEventsEnabled) != 0;
}

void JourneyState::enableEvents()
{
    setFlags0(FlagEventsEnabled);
}

void JourneyState::enableEventsAndCheck()
{
    handleRaiseEvents(setFlags0(FlagEventsEnabled));
}

bool JourneyState::isDone() const
{
    int state = state_.load(std::memory_order_relaxed);
    return (state & FlagDone) != 0;
}

void JourneyState::done()
{
    int old = setFlags0(FlagDone | FlagEntered);
    if ((old & FlagEntered) == 0)
    {
        GLOG("proceed on done: " << stateToString(old));
        incStat<JourneyStat::ProceedOnDone>();
        proceed();
    }
}

void JourneyState::handleEvents()
{
    int event = state_.load(std::memory_order_relaxed);
    if (event & FlagEventsEnabled)
    {
        VERIFY((event & (FlagCancelled | FlagTimedout | FlagDone)) != 0,
               "Must have event");
        handleRaiseEvents(event);
        // goes to FlagDone resetting on exception absence
    }
    else
    {
        VERIFY((event & FlagDone) != 0, "Must be done");
    }
    resetFlags0(FlagDone);
}

void JourneyState::resetEnteredAndCheckEvents()
{
    int prev = state_.load(std::memory_order_relaxed);
    do
    {
        if (mayProceed(prev))
        {
            GLOG("proceed in resetEnteredAndCheckEvents: " << stateToString(prev));
            incStat<JourneyStat::ProceedOnExit>();
            proceed();
            return;
        }
    } while (!state_.compare_exchange_weak(prev, prev & ~FlagEntered,
                                           std::memory_order_relaxed));
    GLOG("reset entered, was: " << stateToString(prev));
}

bool JourneyState::acquire(int oldCounter, bool wasEventsEnabled)
{
    if (!wasEventsEnabled)
    {
        // we don't need to check the counter because
        // the only case when we can proceed is by proceed with the only done
        GLOG("events was disabled => acquired counter: " << oldCounter);
        incStat<JourneyStat::AcquiredOnDisabledEvents>();
        return true;
    }
    int prev = state_.load(std::memory_order_relaxed);
    do
    {
        int counter = prev & MaskCounter;
        if (oldCounter != counter)
        {
            GLOG("event occured, cannot acquire: expected counter: " << oldCounter << ", " << stateToString(prev));
            incStat<JourneyStat::AcquiredFailed>();
            return false;
        }
    } while (!state_.compare_exchange_weak(prev, prev & ~FlagEventsEnabled,
                                           std::memory_order_relaxed));
    GLOG("reset entered, was: " << stateToString(prev));
    incStat<JourneyStat::Acquired>();
    return true;
}

void JourneyState::releaseAndDone(bool wasEventsEnabled)
{
    if (!wasEventsEnabled)
    {
        incStat<JourneyStat::ReleaseOnDisabledEvents>();
        done();
        return;
    }
    int old = setFlags0(FlagDone | FlagEntered | FlagEventsEnabled);
    if ((old & FlagEntered) == 0)
    {
        GLOG("proceed on done with events enabled: " << stateToString(old));
        incStat<JourneyStat::ProceedOnReleaseDone>();
        proceed();
    }
    else
    {
        GLOG("release and done, wait to enter: " << stateToString(old));
    }
}

void JourneyState::detachDoers()
{
    // FIXME: here is a race: it can be acquired before counter increasing
    // fix: use only single shot done instead of acq/rel, always inc the counter: for done, event and detach
    // fix, don't work due to done flag inconsistency (cannot perform release operation or mark acq operation)
    // 1.add and check that done flag is reset, if the done is here =>
    // 2.acquire must check for done flag
    state_.fetch_add(ValueCounter, std::memory_order_relaxed);
    int state = resetFlags0(FlagDone);
    GLOG("detached doers: " << stateToString(state));
}

int JourneyState::counter()
{
    return state_.load(std::memory_order_relaxed) & MaskCounter;
}

bool JourneyState::isEventsEnabled()
{
    return (state_.load(std::memory_order_relaxed) & FlagEventsEnabled) != 0;
}

void JourneyState::handleRaiseEvents(int eventFlag)
{
    if (eventFlag & FlagCancelled)
    {
        // cancel has more priority and resets timeout event
        resetFlags0(FlagCancelled | FlagTimedout | FlagDone);
        GLOG("throwing event: cancel");
        incStat<JourneyStat::RaiseCancelEvent>();
        throw CancelledEvent();
    }
    if (eventFlag & FlagTimedout)
    {
        resetFlags0(FlagTimedout | FlagDone);
        GLOG("throwing event: timeout");
        incStat<JourneyStat::RaiseTimeoutEvent>();
        throw TimedoutEvent();
    }
}

void JourneyState::addRaiseEvent(int eventFlag)
{
    incStat<JourneyStat::SendRaiseEvent>();
    int prev = state_.load(std::memory_order_relaxed);
    GLOG("adding raise event: " << statesToString(prev, eventFlag));
    while (true)
    {
        int next = prev | eventFlag;
        if ((prev & FlagEventsEnabled) != 0)
        {
            if (state_.compare_exchange_weak(prev, (next + ValueCounter) | FlagEntered, std::memory_order_relaxed))
            {
                if ((next & FlagEntered) == 0)
                {
                    GLOG("proceed on raise event: " << statesToString(prev, next));
                    incStat<JourneyStat::ProceedOnRaiseEvent>();
                    proceed();
                }
                else
                {
                    GLOG("cannot proceed on raise event: entered: " << statesToString(prev, next));
                }
                return;
            }
        }
        else
        {
            if (state_.compare_exchange_weak(prev, next, std::memory_order_relaxed))
            {
                GLOG("cannot proceed on raise event: disabled: " << statesToString(prev, next));
                return;
            }
        }
    }
}

int JourneyState::resetFlags0(int flags)
{
    int old = state_.fetch_and(~flags, std::memory_order_relaxed);
    GLOG("reset flags: " << statesToString(old, flags));
    return old;
}

int JourneyState::setFlags0(int flags)
{
    int old = state_.fetch_or(flags, std::memory_order_relaxed);
    GLOG("set flags: " << statesToString(old, flags));
    return old;
}

void JourneyState::proceed()
{
    j_.proceed();
}

bool JourneyState::mayProceed(int state)
{
    // either done or has allowed raise events (cancel or timedout)
    return (state & FlagDone) != 0
            || ((state & FlagEventsEnabled) != 0
            && (state & (FlagCancelled | FlagTimedout)) != 0);
}

std::string JourneyState::stateToString(int state)
{
    std::string res = std::to_string((state & MaskCounter) / ValueCounter);
    auto add = [&](int flag, const char* name)
    {
        if ((flag & state) == 0)
            return;
        res += "|";
        res += name;
    };
    add(FlagEntered, "entered");
    add(FlagEventsEnabled, "enabled");
    add(FlagDone, "done");
    add(FlagCancelled, "cancelled");
    add(FlagTimedout, "timedout");
    return res;
}

std::string JourneyState::statesToString(int oldState, int state)
{
    return stateToString(state) + " <: " + stateToString(oldState);
}

Journey::Journey(Handler handler, IScheduler& s)
    : coro{coroHandler(std::move(handler))}
    , sched{&s}
    , indx{++ atomic<struct Index>()}
    , state_{std::make_shared<JourneyState>(*this)}
{
}

/*
void Journey::cleanup()
{
    state_->cancel();
}
*/

Handler Journey::coroHandler(Handler handler)
{
    return [this, handler] {
        JJLOG("started");
        try
        {
            /*
             * It's unsafe to invoke handleEvents before handler
             * The reason is that the user may want to use the RAII object
             * at the beginning of the handler.
             * And it's not obvious the it must be placed inside handler closure
             * as a variables
             * So the user should use reschedule to cancel the coroutine
             */
            handler();
        }
        catch (std::exception& e)
        {
            RJLOG("exception in coro: " << e.what());
        }
        JJLOG("ended");
    };
}

Handler Journey::doneHandler()
{
    Doer d = doer();
    return [d]() mutable {d.done();};
}

void Journey::proceed()
{
    schedule0([this] {
        proceed0();
    });
}

Handler Journey::proceedHandler()
{
    return [this] {
        proceed();
    };
}

void Journey::deferProceed(ProceedHandler procHandler)
{
    procHandler(doneHandler());
    waitForDone();
}

void Journey::teleport(IScheduler& s)
{
    if (&s == sched) {
        // TODO: add stats for skipping check
        JJLOG("the same destination, skipping teleport <-> " << s.name());
        return;
    }
    JJLOG("teleport " << sched->name() << " -> " << s.name());
    sched = &s;
    reschedule();
}

void Journey::waitForDone()
{
    if (state_->isDone())
    {
        JJLOG("the work was done, continuing without rescheduling");
        incStat<JourneyStat::DoneWithoutRescheduling>();
    }
    else
    {
        suspend();
    }
    handleEvents();
}

void Journey::waitForDoneAlways(ICancel &cancel)
{
    if (!state_->isDone())
    {
        suspend();
        if (!state_->isDone())
        {
            JJLOG("need to wait until done: disable all events and invoke cancellation");
            incStat<JourneyStat::ForceDoneByCancel>();
            DtorEventsGuard g;
            cancel.cancel();
            suspend();
        }
    }
    JJLOG("cleanup and handle events");
    cancel.cleanup();
    handleEvents();
}

void Journey::waitForDoneAlwaysGuarded(ICancel &cancel)
{
    DtorEventsGuard g;
    if (!state_->isDone())
    {
        JJLOG("need to wait until done: disable all events and invoke cancellation");
        incStat<JourneyStat::ForceDoneByCancelGuarded>();
        cancel.cancel();
        suspend();
    }
    JJLOG("cleanup and handle events");
    cancel.cleanup();
    handleEvents();
}

void Journey::reschedule()
{
    state_->done();
    suspend();
    handleEvents();
}

void Journey::handleEvents()
{
    // TODO: should we check for uncaught_exception?
    state_->handleEvents();
}

bool Journey::disableEvents()
{
    return state_->disableEvents();
}

void Journey::enableEvents()
{
    state_->enableEvents();
}

void Journey::enableEventsAndCheck()
{
    state_->enableEventsAndCheck();
}

void Journey::detachDoers()
{
    state_->detachDoers();
}

IScheduler& Journey::scheduler() const
{
    return *sched;
}

GC &Journey::gc()
{
    return gc_;
}

int Journey::index() const
{
    return indx;
}

Goer Journey::goer() const
{
    return Goer{state_};
}

Doer Journey::doer() const
{
    return Doer{*state_};
}

DetachableDoer Journey::detachableDoer() const
{
    return DetachableDoer{state_};
}

Goer Journey::start(Handler handler, IScheduler& s)
{
    /*
     * Two-phase init due to:
     * 1. exception safety guarantee
     * 2. goer must be returned before starting to avoid races
     */
    return (new Journey(std::move(handler), s))->start0();
}

Goer Journey::start0()
{
    Goer g = goer();
    proceed();
    return g;
}

void Journey::schedule0(Handler handler)
{
    VERIFY(sched != nullptr, "Scheduler must be set in journey");
    /*
     * TODO: consider use sync behavior on forced cancel, use it accurately
     */
    if (t_cancellation != 0 && tlsPtr<Journey>() != nullptr && tlsPtr<Journey>()->sched == sched)
    {
        JJLOG("schedule handler sync: " << t_cancellation);
        handler();
    }
    else
    {
        sched->schedule(std::move(handler));
    }
}

void Journey::proceed0()
{
    {
        auto _ = tlsGuard(this);
        bool isCompleted = coro.resume();
        if (isCompleted)
        {
            delete this;
            return;
        }
    }
    state_->resetEnteredAndCheckEvents();
}

void Journey::suspend()
{
    JJLOG("journey suspending");
    incStat<JourneyStat::Suspended>();
    incStat<JourneyStat::CoroYield>();
    coro.yield();
    decStat<JourneyStat::Suspended>();
    JJLOG("journey resumed");
}

Journey& journey()
{
    return tls<Journey>();
}

void waitForAll()
{
    TLOG("waiting for journeys to complete");
    waitFor([] { return Journey::createCounter() == Journey::destroyCounter(); });
    TLOG("waiting for journeys completed");
}

void incCancellation()
{
    ++t_cancellation;
}

void decCancellation()
{
    --t_cancellation;
}

}
