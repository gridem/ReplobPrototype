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

namespace coro
{

// TODO: const -> constexpr
// TODO: enum -> enum class
// TODO: use consts for sizes (packet limit, buffer limit etc), use c_{const} definition

constexpr size_t c_stackSize = 1024 * 32;

Coro::Coro(Handler h)
    : stack(c_stackSize)
    , handler(std::move(h))
{
    coroContext = boost::context::make_fcontext(
                &stack.back(), stack.size(), &starterWrapper0);
}

Coro::~Coro() noexcept
{
    if (!isCompleted())
    {
        RLOG("destroying started coro in state: "
             << (state == State::Running ? "running" : "suspended"));
    }
}

// continue coroutine execution after yield
bool Coro::resume()
{
    VERIFY(state == State::Suspended, "Coro must be suspended");
    resume0(reinterpret_cast<intptr_t>(this));
    return state == State::Completed;
}

// yields execution to external context
void Coro::yield()
{
    VERIFY(state == State::Running, "Coro must be running");
    state = State::Suspended;
    yield0();
    state = State::Running;
}

// is coroutine was started and not completed
bool Coro::isCompleted() const noexcept
{
    return state == State::Completed;
}

// returns to external context
void Coro::yield0() noexcept
{
    boost::context::jump_fcontext(&coroContext, externalContext, 0);
}

void Coro::resume0(intptr_t p) noexcept
{
    boost::context::jump_fcontext(&externalContext, coroContext, p);
}

void Coro::starterWrapper0(intptr_t p) noexcept
{
    reinterpret_cast<Coro*>(p)->starter0();
}

void Coro::starter0() noexcept
{
    state = State::Running;
    /*
     * If exception is thrown inside handler then
     * we don't have a consistent state
     * but usually after that the application is crashed
     * so it makes no sense to do it using RAII
     */
    handler();
    state = State::Completed;
    yield0();
}

}
