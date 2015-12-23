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

#pragma once

#include <boost/asio.hpp>
#include <boost/context/all.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <iostream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <synca/synca.h>
#include <synca/log.h>

#include "stats.h"
#include "coro.h"
#include "journey.h"

namespace synca {

struct AsioService : boost::asio::io_service
{
};

typedef boost::system::error_code ErrorCode;

}

