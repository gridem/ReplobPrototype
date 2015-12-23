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

/*
 * TODO: use uniform member access (dsfsdf_)
 */

#define DECL_EXC(D_name, D_base, D_msg) \
    struct D_name : D_base \
    { \
        D_name() : D_base{D_msg} {} \
        D_name(const std::string& msg) : D_base{D_msg ": " + msg} {} \
    };

DECL_EXC(Error, std::runtime_error, "Synca error")
DECL_EXC(Event, Error, "Event received")
DECL_EXC(CancelledEvent, Event, "Cancel")
DECL_EXC(TimedoutEvent, Event, "Timeout")

struct JourneyState;

// TODO: only Goer should be available for user. consider move to journey
// TODO: consider using with default ctor by using journey()
struct Doer
{
    Doer();
    explicit Doer(JourneyState& state);

    void done();

private:
    JourneyState& state_;
};

struct Goer
{
    Goer();
    explicit Goer(const std::shared_ptr<JourneyState>& state);

    void cancel();
    void timedout();

private:
    // TODO: consider weak_ptr
    std::shared_ptr<JourneyState> state_;
};

// TODO: split implementation for: done/detach(need +1) and acquire/release(no need +1)
struct DetachableDoer
{
    DetachableDoer();
    explicit DetachableDoer(const std::shared_ptr<JourneyState>& state);

    // TODO: think about RAII semantics on acquire/done
    bool acquire();
    void releaseAndDone();
    void done(); // done only if acquired();

private:
    std::shared_ptr<JourneyState> state_;
    int counter_;
    bool eventsEnabled_;
};

}
