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

// TODO: consider net namespace?

DECL_EXC(NetworkError, Error, "Network error")

struct NetworkTag;

using Port = unsigned short;

struct View
{
    Ptr data;
    size_t size;
};

struct Endpoint
{
    enum class Type
    {
        V4,
        V6,
    };

    Port port;
    std::string address;
    Type type;
};

struct Acceptor;
struct Socket
{
    friend struct Acceptor;

    Socket();

    void read(View);
    size_t partialRead(View);
    void write(View);
    void write(std::initializer_list<View>);
    void connect(const Endpoint& e);
    void close();

    struct Impl;
private:
    std::shared_ptr<Impl> socket;
};

typedef std::function<void(Socket&)> SocketHandler;
struct Acceptor
{
    explicit Acceptor(Port port, Endpoint::Type type = Endpoint::Type::V4);

    Socket accept();
    void goAccept(SocketHandler);

    struct Impl;
private:
    std::shared_ptr<Impl> acceptor;
};

struct Resolver
{
    Resolver();
    std::vector<Endpoint> resolve(const std::string& hostname, Port port);
    struct Impl;
private:
    std::shared_ptr<Impl> resolver;
};

}
