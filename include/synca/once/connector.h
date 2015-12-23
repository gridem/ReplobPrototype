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

template<typename T_error, typename F1, typename F2>
void retry(F1 action, F2 onRetry)
{
    while (true)
    {
        try
        {
            action();
            break;
        }
        catch (T_error& e)
        {
            JLOG("retrying on: " << e.what());
        }
        onRetry();
    }
}

const auto networkRetry = [](auto action, auto onRetry) { retry<NetworkError>(action, onRetry); };

DECL_EXC(ConnectorError, Error, "Connector")

/*
template<typename F1, typename F2>
void networkRetry(F1 action, F2 onFail)
{
    while (true)
    {
        try
        {
            action();
            break;
        }
        catch (NetworkError& e)
        {
            LOG("network error: " << e.what() << ", retry");
        }
        onFail();
    }
}
*/

struct Connector
{
    explicit Connector(const Endpoint& e);
    ~Connector();

    void write(View);
    void write(std::initializer_list<View>);
    void disconnect();

private:
    void reconnect();

    template<typename F>
    void performOp(F f)
    {
        networkRetry(f, [&] { reconnect(); });
    }

    Socket socket_;
    Endpoint endpoint_;
    Mutex write_;
    bool disconnected_ = false;
    //Atomic<bool> disconnected_;
};

}
