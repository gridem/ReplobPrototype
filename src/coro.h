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

namespace coro {
/*
 * TODO: coro: remove exceptions saving, remove try
 * TODO: mark non exception functions as noexcept to guarantee features
 * TODO: add creating without start: only resume starts: single point of start
 * TODO: add listener<...> = channel<variant<...>>
 */

// coroutine class
struct Coro
{
    // Create coroutine without starting
    // need to execute resume function to start it.
    // User is responsible to provide exception-safe handler
    explicit Coro(Handler);

    ~Coro() noexcept;

    // continue coroutine execution after yield
    // returns true on completed execution
    bool resume();

    // is coroutine was completed
    bool isCompleted() const noexcept;

    // return back from coroutine
    void yield();

private:
    void yield0() noexcept;
    void resume0(intptr_t p = 0) noexcept;
    void starter0() noexcept;

    static void starterWrapper0(intptr_t p) noexcept;

    enum class State
    {
        Suspended,
        Running,
        Completed,
    };

    State state {State::Suspended};

    boost::context::fcontext_t coroContext;
    boost::context::fcontext_t externalContext;
    std::vector<uint8_t> stack;
    Handler handler;
};

}
