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

void Listener::listen(Port port, SocketHandler handler)
{
    gr_ = go([port, handler] {
        Acceptor a{port};
        while (true)
            a.goAccept(handler);
    });
}

void Listener::cancel()
{
    // TODO: need to cancel goAccept(handler) either
    gr_.cancel();
}

}
