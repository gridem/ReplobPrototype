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

namespace synca {

struct ConnectorStat
{
    struct Write {};
    struct WriteList {};
    struct Reconnect {};
    struct ConnectRetry {};
    struct Disconnected {};
};

Connector::Connector(const Endpoint &e)
    : endpoint_(e)
{
}

Connector::~Connector()
{
    disconnect();
}

void Connector::write(View view)
{
    incStat<ConnectorStat::Write>();
    Lock _{write_};
    performOp([&] {
        socket_.write(view);
    });
}

void Connector::write(std::initializer_list<View> views)
{
    incStat<ConnectorStat::WriteList>();
    Lock _{write_};
    performOp([&] {
        socket_.write(views);
    });
}

void Connector::disconnect()
{
    //disconnected_.store(true, std::memory_order_relaxed);
    if (disconnected_)
        return;
    incStat<ConnectorStat::Disconnected>();
    disconnected_ = true;
    socket_.close();
}

void Connector::reconnect()
{
    incStat<ConnectorStat::Reconnect>();
    networkRetry([&] {
        //if (disconnected_.load(std::memory_order_relaxed))
        if (disconnected_)
            throw ConnectorError("Disconnected");
        socket_.connect(endpoint_);
    }, [&] {
        incStat<ConnectorStat::ConnectRetry>();
    });
}

}

