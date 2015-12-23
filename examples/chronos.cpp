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

#include <queue>

using namespace std::chrono_literals;

// TODO: use steady clock for detector and system clock for Chronos!!!
using Clock = std::chrono::steady_clock;
using TimeStamp = Clock::time_point;
using TimeDiff = Clock::duration;

#define DLOG(D_msg)         JLOG("DETECTOR: " << D_msg)
#define RDLOG(D_msg)        RJLOG("DETECTOR: " << D_msg)

void onNodeRemove(NodeId id);

struct Detector
{
    Detector()
    {
        auto now = Clock::now();
        for (NodeId n: single<NodesConfig>().otherNodes())
        {
            nodesTs_[n] = now;
        }
    }

    void start()
    {
        timer.reset(500, [this] {
            pulse();
        });
    }

    void pulse()
    {
        DLOG("pulse");
        MCleanup {
            start();
        };
        NodeId id = thisNode();
        broadcast([id] {
            single<Detector>().update(id);
        });
        check();
    }

    void update(NodeId id)
    {
        DLOG("update node: " << id);
        nodesTs_[id] = Clock::now();
    }

    void check()
    {
        auto now = Clock::now();
        std::vector<NodeId> toRemove;
        for (auto&& n: nodesTs_)
        {
            auto&& id = n.first;
            auto&& ts = n.second;
            if (now - ts > 3s)
                toRemove.push_back(id);
        }
        for (NodeId n: toRemove)
            removeNode(n);
    }

    void removeNode(NodeId id)
    {
        if (nodesTs_.count(id) == 0)
            return;
        DLOG("removing node: " << id);
        replobApply([id] {
            // rescheduling is not allowed inside transaction!
            // thus we need to do it in async way
            go([id] {
                single<Detector>().removeNodeComplete(id);
            });
        });
    }

    void removeNodeComplete(NodeId id)
    {
        if (nodesTs_.count(id) == 0)
            return;
        RDLOG("completing removing node: " << id);
        nodesTs_.erase(id);
        single<Nodes>().remove(id);
        single<NodesConfig>().removeNode(id);
        onNodeRemove(id);
    }

private:
    GlobalTimer timer;
    std::unordered_map<NodeId, TimeStamp> nodesTs_;
};

#define CLOG(D_msg)         JLOG("CHRONOS: " << D_msg)
#define RCLOG(D_msg)        RJLOG("CHRONOS: " << D_msg)

template<typename T_container, typename T_element>
void removeElement(T_container& c, T_element&& e)
{
    c.erase(std::remove(c.begin(), c.end(), std::forward<T_element>(e)), c.end());
}

struct Chronos
{
    Chronos()
    {
        availableNodes_ = single<NodesConfig>().nodes();
        // each node can execute only single job at a time
        for (NodeId id: availableNodes_)
            slots_.push_back(id);
    }

    void addEvent(TimeStamp ts, Handler h)
    {
        CLOG("add event");
        events_.push({ts, std::move(h)});
        scheduleWait();
    }

    void addEvent(TimeDiff diff, Handler h)
    {
        addEvent(diff + Clock::now(), std::move(h));
    }

    void onNodeRemove(NodeId id)
    {
        RCLOG("on node remove: " << id);
        removeElement(availableNodes_, id);
        removeElement(slots_, id);
        if (running_.count(id) == 0)
            return;
        awaiting_.push(std::move(running_[id]));
        running_.erase(id);
        triggerAwaitings();
    }

private:
    void scheduleWait()
    {
        if (events_.empty())
        {
            nextEvent_.reset();
            return;
        }
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    events_.top().ts - Clock::now()).count();
        if (diff < 0)
            scheduleEvents();
        else
            nextEvent_.reset(diff + 100 /*100ms shift*/, [] {
                MReplobTransactLocalInstance(Chronos) {
                    $.scheduleEvents();
                };
            });
    }

    void triggerAwaitings()
    {
        CLOG("trigger awaitings");
        while (!awaiting_.empty() && !slots_.empty())
        {
            executeHandler(awaiting_.front(), slots_.front());
            awaiting_.pop();
            slots_.pop_front();
        }
    }

    void scheduleEvents() const
    {
        CLOG("schedule events");
        auto now = Clock::now();
        if (!events_.empty() && events_.top().ts < now)
        {
            // we cannot do it immediately due to reasons:
            // 1. it's const method for local transactions on timer event
            // 2. now isn't stable across nodes thus we need to commit that time to act uniformely
            //    (timer is not deterministic and thus it cannot be used inside transaction)
            go([now] {
                MReplobTransactInstance(Chronos) {
                    $.scheduleEventsUpTo(now);
                };
            });
        }
    }

    void scheduleEventsUpTo(TimeStamp now)
    {
        CLOG("schedule events up to");
        while (!events_.empty() && events_.top().ts < now)
        {
            awaiting_.push(std::move(events_.top().h));
            events_.pop();
        }
        triggerAwaitings();
        scheduleWait();
    }

    bool executeHandler(Handler h, NodeId id)
    {
        CLOG("execute handler");
        running_[id] = h;
        if (thisNode() != id)
            return false;
        CLOG("perform async handler execution");
        go([h, id] {
            h();
            MReplobTransactInstance(Chronos) {
                $.onHandlerCompleted(id);
            };
        });
        return true;
    }

    void onHandlerCompleted(NodeId id)
    {
        CLOG("on handler completed");
        running_.erase(id);
        slots_.push_back(id);
        triggerAwaitings();
    }

    struct Event
    {
        TimeStamp ts;
        Handler h;

        bool operator<(const Event& e) const
        {
            return ts > e.ts;
        }
    };

    std::vector<NodeId> availableNodes_;
    std::priority_queue<Event> events_;
    std::unordered_map<NodeId, Handler> running_;
    std::queue<Handler> awaiting_;
    std::deque<NodeId> slots_;
    GlobalTimer nextEvent_;
};

void onNodeRemove(NodeId id)
{
    /*
    go([id] {
        MReplobTransactInstance(Chronos) {
            $.onNodeRemove(id);
        };
    });
    */
    // actually node can be removed inside this function
    // because it is invoked withing replob transaction
    single<Chronos>().onNodeRemove(id);
}

void addActionAfterInterval(const char* event, TimeDiff diff, Handler h = [] {})
{
    MReplobTransactInstance(Chronos) {
        $.addEvent(diff, [=] {
            RJLOG("EVENT " << event);
            h();
            RJLOG("EVENT " << event << " completed");
        });
    };
}

void addSleepAndAction(const char* event, Handler h = [] {})
{
    MReplobTransactInstance(Chronos) {
        $.addEvent(1s, [=] {
            RJLOG("EVENT " << event);
            sleepa(1000);
            h();
            RJLOG("EVENT " << event << " completed");
        });
    };
}

void example1()
{
    addSleepAndAction("1", [] {
        addSleepAndAction("2", [] {
            RJLOG("HELLO");
        });
        addSleepAndAction("3", [] {
            RJLOG("WORLD");
        });
        RJLOG("HELLO WORLD scheduled");
    });
}

void example2()
{
    addActionAfterInterval("3", 3s);
    addActionAfterInterval("1", 1s);
    addActionAfterInterval("2", 2s);
}

// TODO: use "ms" postfix for time instead of ms explicitly
constexpr int c_concurrent = 3;
constexpr int c_amount = 50;

void scheduleAction(int i)
{
    if (i < 0)
        return;
    addActionAfterInterval("", 3s, [i] {
        RJLOG("event: " << i);
        sleepa(1000);
        scheduleAction(i - c_concurrent);
    });
}

void example3()
{
    for (int i = 0; i < c_concurrent; ++ i)
    {
        go([i] {
            scheduleAction(c_amount - i);
        });
    }
}

void chronos()
{
    single<Detector>().start();
    if (thisNode() != 1)
        return;
    example3();
}

/*
 * how to stop:
 * 1. go handler with asleep period
 * 2. at the end shutdown
 * 3. if the operation completed: broadcast cancel of asleep handler
 */
void starter()
{
    if (thisNode() == 1)
    {
        go ([] {
            broadcast(chronos);
            chronos();
        });
    }
    sleepFor(500 * 1000);
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
