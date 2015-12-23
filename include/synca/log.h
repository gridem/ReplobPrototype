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

#include <iostream>
#include <synca/common.h>

//TODO: improve logging (disable/enable per component, function names, src files etc, backends)
//TODO: refactor logs to speedup compilation

// Internal macro
#define SLOG__(D_msg)               std::cerr << currentTimeDate() << ": " << D_msg << std::endl

// Release log
#ifdef flagLOG_MUTEX
#   include <mutex>
#   define RLOG(D_msg)              do { std::lock_guard<std::mutex> _(single<std::mutex>()); SLOG__(D_msg); } while(false)
#else
#   define RLOG                     SLOG__
#endif

// Null log
#define NLOG(D_msg)

#ifdef flagLOG_DEBUG
#   define LOG                      RLOG
#else
#   define LOG                      NLOG
#endif

#define  TLOG(D_msg)              LOG(synca::name() << "#" << synca::number() << ": " << D_msg)
#define RTLOG(D_msg)             RLOG(synca::name() << "#" << synca::number() << ": " << D_msg)
#define  JLOG(D_msg)             TLOG("[" << synca::index() << "] " << D_msg)
#define RJLOG(D_msg)            RTLOG("[" << synca::index() << "] " << D_msg)
//#define  GLOG(D_msg)             JLOG("-> [" << index_ << "]: " << D_msg)
#define GLOG NLOG
