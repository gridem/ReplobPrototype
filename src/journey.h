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

struct IProceed : IObject
{
    virtual void proceed() = 0;
};

enum class EnableEventsPolicy
{
    CheckOnEnable,
    DontCheckOnEnable,      // helpful inside dtor
};

struct GC
{
    ~GC()
    {
        for (auto& deleter: boost::adaptors::reverse(deleters))
            deleter();
    }

    void add(detail::TypeAction t)
    {
        deleters.emplace_back(t);
    }

private:
    std::vector<detail::TypeAction> deleters;
};

// TODO: move to separate file
struct Journey;
struct JourneyState
{
    explicit JourneyState(Journey& j);

    enum
    {
        FlagEntered = 1 << 0,
        FlagEventsEnabled = 1 << 1,
        FlagDone = 1 << 2,
        FlagCancelled = 1 << 3, // raise event
        FlagTimedout = 1 << 4, // raise event
        ValueCounter = 1 << 5,
        MaskCounter = ~(ValueCounter - 1), // the rest is the counter
    };

    void cancel();
    void timedout();
    // returns: was events enabled?
    bool disableEvents();
    void enableEvents();
    void enableEventsAndCheck();
    bool isDone() const;
    bool isEventsEnabled();
    void done();
    void handleEvents();
    void resetEnteredAndCheckEvents();
    bool acquire(int oldCounter, bool wasEventsEnabled);
    void releaseAndDone(bool wasEventsEnabled);
    void detachDoers();
    int counter();

private:
    void handleRaiseEvents(int eventFlag);
    void addRaiseEvent(int eventFlag);
    int resetFlags0(int flags);
    int setFlags0(int flags);
    void proceed();
    static bool mayProceed(int state);
    static std::string stateToString(int state);
    static std::string statesToString(int oldState, int state);

    // TODO: uint64_t or uint32_t?
    Atomic<int> state_ {FlagEntered | FlagEventsEnabled/* | FlagDone*/}; // FlagDone MUST be used only if handleEvents before handler
    Journey& j_;
    int index_; // to avoid races when we try to send an event and put logs, TODO: consider removing
};

// TODO: journey cannot be used together WithCleanup because it doesn't remove deleted instances
struct Journey : InstanceStat<Journey>//, WithCleanup// : IProceed
{
    void proceed();
    Handler doneHandler();
    void deferProceed(ProceedHandler proceed);
    void teleport(IScheduler& s);
    void waitForDone();
    void waitForDoneAlways(ICancel& cancel); // TODO: consider to move cancellation to destructor of caller
    void waitForDoneAlwaysGuarded(ICancel& cancel);
    void reschedule();

    void handleEvents();
    bool disableEvents();
    void enableEvents();
    void enableEventsAndCheck();
    void detachDoers();

    IScheduler& scheduler() const;
    GC& gc();
    int index() const;
    Goer goer() const;
    Doer doer() const;
    DetachableDoer detachableDoer() const;

    static Goer start(Handler handler, IScheduler& s);
    
private:
    Journey(Handler handler, IScheduler& s);
    ~Journey() {}

    //void cleanup() override;

    Handler proceedHandler();
    Handler coroHandler(Handler);
    Goer start0();
    void schedule0(Handler);
    void proceed0();
    void suspend();

    coro::Coro coro;
    IScheduler* sched = nullptr;
    int indx = -1; // TODO: use index from state_
    Journey* prev = nullptr;
    std::shared_ptr<JourneyState> state_; // consider adding to the list to debug or dump the state

    // TODO: consider using cls: coro local storage
    GC gc_;
};

Journey& journey();

void incCancellation();
void decCancellation();

}
