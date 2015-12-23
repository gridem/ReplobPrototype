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

macro(configure_msvc_runtime)
    if(MSVC)
        # Default to statically-linked runtime.
        if("${MSVC_RUNTIME}" STREQUAL "")
            set(MSVC_RUNTIME "dynamic")
        endif()

        # Set compiler options.
        set(variables
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO)

        if(${MSVC_RUNTIME} STREQUAL "static")
            message(STATUS "MSVC: using statically-linked runtime (/MT and /MTd).")
            foreach(variable ${variables})
                if(${variable} MATCHES "/MD")
                    string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
                endif()
            endforeach()
        else()
            message(STATUS "MSVC: using dynamically-linked runtime (/MD and /MDd).")
            foreach(variable ${variables})
                if(${variable} MATCHES "/MT")
                    string(REGEX REPLACE "/MT" "/MD" ${variable} "${${variable}}")
                endif()
            endforeach()
        endif()

        foreach(variable ${variables})
            if(${variable} MATCHES "/Ob0")
                string(REGEX REPLACE "/Ob0" "/Ob2" ${variable} "${${variable}}")
            endif()
        endforeach()

        foreach(variable ${variables})
            set(${variable} "${${variable}}" CACHE STRING "MSVC_${variable}" FORCE)
        endforeach()
    endif()
endmacro(configure_msvc_runtime)
