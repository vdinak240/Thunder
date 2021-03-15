/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Module.h"

namespace WPEFramework {
namespace WarningReporting {

    class EXTERNAL TooLongWaitingForLock {
    private:
        // -------------------------------------------------------------------
        // This object should not be copied or assigned. Prevent the copy
        // constructor and assignment constructor from being used. Compiler
        // generated assignment and copy methods will be blocked by the
        // following statments.
        // Define them but do not implement them, compile error/link error.
        // -------------------------------------------------------------------
        TooLongWaitingForLock(const TooLongWaitingForLock& a_Copy) = delete;
        TooLongWaitingForLock& operator=(const TooLongWaitingForLock& a_RHS) = delete;

    public:

        TooLongWaitingForLock(uint64_t duration, const TCHAR formatter[], ...)
        : _text()
        {
            std::string message;

            va_list ap;
            va_start(ap, formatter);
            Core::Format(message, formatter, ap);
            va_end(ap);
            
            _text = Core::Format(_T("duration %" PRIu64 "ms, %s"), duration, message.c_str());
        }

        explicit TooLongWaitingForLock(uint64_t duration)
        : _text()
        {
            _text = Core::Format(_T("duration %" PRIu64 "ms"), duration);
        }

        ~TooLongWaitingForLock() = default;

    public:

        inline static uint64_t MaxAllowedDurationMs() {
            return 50; // todo huppel make configurable
        }

        inline const char* Data() const
        {
            return (_text.c_str());
        }
        inline uint16_t Length() const
        {
            return (static_cast<uint16_t>(_text.length()));
        }

    private:
        std::string _text;
    };

}
} 


