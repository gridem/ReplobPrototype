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

#include <boost/optional.hpp>

#include <vector>
#include <stdexcept>
#include <memory>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>

#include "common.h"

//// TODO: consider refactor to avoid <mutex> etc
#include "mutex.h"
#include "log.h"
////

#include "once/cleanup.h"
#include "once/thread_pool.h"
#include "once/oers.h"
#include "once/core.h"
#include "once/gc.h"
#include "once/portal.h"
#include "once/network.h"
#include "once/data.h"
#include "once/connector.h"
#include "once/listener.h"
#include "once/node.h"
#include "once/serialization.h" // TODO: consider move to impl
#include "once/modifiers.h"
