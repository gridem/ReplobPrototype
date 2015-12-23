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

#include <synca/adapter.h>

#include "synca_impl.h"

/*
 * TODO: add wrapper to wrap all async calls using unified interface: wrap(cb)
 * TODO: remove defer
 */

DECL_ADAPTER(synca::Socket::Impl, read, write, partialRead, connect)

DECL_ADAPTER(synca::Acceptor::Impl, accept)

DECL_ADAPTER(synca::Resolver::Impl, resolve)

namespace synca {

NetworkError codeToError(const ErrorCode& e)
{
    return NetworkError{boost::system::system_error(e).what()};
}

auto toBuf(View view)
{
    return boost::asio::buffer(view.data, view.size);
}

auto toBuf(std::initializer_list<View> views)
{
    std::vector<boost::asio::const_buffer> bufs;
    bufs.reserve(views.size());
    for (auto&& v: views)
        bufs.push_back(toBuf(v));
    return bufs;
}

struct CloseOp
{
    template<typename T>
    static void apply(T& t) { t.close(); }
};

struct CancelOp
{
    template<typename T>
    static void apply(T& t) { t.cancel(); }
};

template<typename T, typename T_close = CloseOp>
struct CloseOnError
{
    explicit CloseOnError(T& t) : t_{t} {}
    ~CloseOnError()
    {
        if (wasDone_)
            return;
        DtorEventsGuard _;
        close();
        wait();
    }

    void wait()
    {
        waitForDone();
    }

    void done()
    {
        wasDone_ = true; //FIXME: race here
        doer_.done();
    }

    void close()
    {
        T_close::apply(t_);
    }

private:
    T& t_;
    bool wasDone_ = false;
    Doer doer_;
};

template<typename T_socket, typename T_type = size_t, typename T_close = CloseOp>
struct BaseSocket : T_socket
{
protected:
    template<typename F, typename... V>
    T_type call(F f, V&&... v)
    {
        ErrorCode e;
        T_type val;
        CloseOnError<T_socket, T_close> closer{*this};
        f(*this, std::forward<V>(v)..., [&closer, &e, &val](const ErrorCode& error, T_type value) {
            e = error;
            val = value;
            closer.done();
        });
        closer.wait();
        if (e)
        {
            closer.close();
            throw codeToError(e);
        }
        return val;
    }
};

template<typename T_socket>
struct BaseAsio : T_socket
{
protected:
    template<typename... V, typename F>
    void call(F f, V&&... v)
    {
        ErrorCode e;
        CloseOnError<T_socket> closer{*this};
        f(*this, std::forward<V>(v)..., [&closer, &e](const ErrorCode& error) {
            e = error;
            closer.done();
        });
        closer.wait();
        if (e)
        {
            closer.close();
            throw codeToError(e);
        }
    }
};

template<typename T, typename U>
T& safe_downcast(U& u)
{
    static_assert(sizeof(T) == sizeof(U), "Unsafe downcast");
    return static_cast<T&>(u);
}

template<typename T_base, typename T>
auto& adapt(T& t)
{
    return safe_downcast<Adapter<T, T_base>>(t);
}

struct Socket::Impl : boost::asio::ip::tcp::socket, InstanceStat<Socket>
{
    using boost::asio::ip::tcp::socket::socket;

    template<typename F>
    void read(View view, F f)
    {
        boost::asio::async_read(*this, toBuf(view), f);
    }

    template<typename F>
    void partialRead(View view, F f)
    {
        async_read_some(toBuf(view), f);
    }

    template<typename T_view, typename F>
    void write(T_view view, F f)
    {
        boost::asio::async_write(*this, toBuf(view), f);
    }

    template<typename F>
    void connect(const Endpoint& e, F f)
    {
        async_connect(
            boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(e.address), e.port),
            f);
    }

    auto& ioOp()
    {
        return adapt<BaseSocket<Socket::Impl>>(*this);
    }

    auto& op()
    {
        return adapt<BaseAsio<Socket::Impl>>(*this);
    }
};

Socket::Socket() : socket{std::make_shared<Impl>(service<NetworkTag>())} {}

void Socket::read(View view)
{
    socket->ioOp().read(view);
}

size_t Socket::partialRead(View view)
{
    return socket->ioOp().partialRead(view);
}

void Socket::write(View view)
{
    socket->ioOp().write(view);
}

void synca::Socket::write(std::initializer_list<synca::View> views)
{
    socket->ioOp().write(views);
}

void Socket::connect(const Endpoint& e)
{
    socket->op().connect(e);
}

void Socket::close()
{
    //socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
    ErrorCode e;
    socket->close(e); // ignore error
}

struct Acceptor::Impl : boost::asio::ip::tcp::acceptor, InstanceStat<Acceptor>
{
    using boost::asio::ip::tcp::acceptor::acceptor;

    template<typename F>
    void accept(Socket& s, F f)
    {
        async_accept(*s.socket, f);
    }
};

Acceptor::Acceptor(Port port, Endpoint::Type type)
    : acceptor{std::make_shared<Impl>(service<NetworkTag>(),
        boost::asio::ip::tcp::endpoint(type == Endpoint::Type::V4
            ? boost::asio::ip::tcp::v4() : boost::asio::ip::tcp::v6(), port))}
{
}

Socket Acceptor::accept()
{
    Socket socket;
    adapt<BaseAsio<Acceptor::Impl>>(*acceptor).accept(socket);
    return socket;
}

void Acceptor::goAccept(SocketHandler handler)
{
    auto socket = accept();
    go([socket, handler]() mutable {
        handler(socket);
    });
}

struct Resolver::Impl : boost::asio::ip::tcp::resolver
{
    using boost::asio::ip::tcp::resolver::resolver;

    template<typename... V>
    void resolve(V&&... v)
    {
        async_resolve(std::forward<V>(v)...);
    }
};

Resolver::Resolver() : resolver{std::make_shared<Impl>(service<NetworkTag>())} {}

std::vector<Endpoint> Resolver::resolve(const std::string& hostname, Port port)
{
    using boost::asio::ip::tcp;

    std::vector<Endpoint> result;
    tcp::resolver::query query(hostname, std::to_string(port));
    tcp::resolver::iterator ends = adapt<BaseSocket<Resolver::Impl, tcp::resolver::iterator, CancelOp>>(*resolver).resolve(query);
    for (tcp::resolver::iterator end; ends != end; ++ ends)
    {
        tcp::endpoint ep = *ends;
        result.emplace_back(Endpoint{ep.port(), ep.address().to_string(),
            ep.protocol() == tcp::v4() ? Endpoint::Type::V4 : Endpoint::Type::V6});
    }
    return result;
}

}
