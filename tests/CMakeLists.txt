# Copyright 2015 Grigory Demchenko (aka gridem)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.8)

function(add_ut NAME)
    add_executable(${NAME} ${NAME}.cpp ${SYNCA_HDR})
    target_link_libraries(${NAME} synca)
endfunction(add_ut)

add_ut(simple_tests)
add_ut(serialization_tests)
add_ut(serialization2_tests)
add_ut(teleport_tests)
add_ut(wait_tests)
add_ut(other_tests)
add_ut(fibo_tests)
add_ut(channel_tests)
add_ut(mutex_tests)
add_ut(md5_bruteforce)
add_ut(adapter_tests)
add_ut(network_tests)
add_ut(phantom_tests)
add_ut(integral_tests)
add_ut(integral2_tests)
add_ut(emulator_tests)
