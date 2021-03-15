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

#include "WarningReportingControl.h"
#include "Sync.h"
#include "Singleton.h"
#include "Thread.h"

namespace {
    WPEFramework::Core::CriticalSection adminlock; // we cannot have this as a member as Sync.h might also need WarningReporting. but as WarningReportingUnitProxy that is not a problem
}

namespace WPEFramework {
namespace WarningReporting {

    const char* CallsignTLS::Callsign() {

        Core::ThreadLocalStorageType<CallsignTLS>& instance = Core::ThreadLocalStorageType<CallsignTLS>::Instance();
        const char* name = nullptr;
        if( ( instance.IsSet() == true ) && ( instance.Context().Name() != nullptr ) ) {
            name = instance.Context().Name(); // should be safe, nobody should for this thread be able to change this while we are using it 
        }
        return name;
    }

    void CallsignTLS::Callsign(const char* callsign) {
        Core::ThreadLocalStorageType<CallsignTLS>::Instance().Context().Name(callsign);
    }

    WarningReportingUnitProxy& WarningReportingUnitProxy::Instance()
    {
        return (Core::SingletonType<WarningReportingUnitProxy>::Instance());
    }

    void WarningReportingUnitProxy::ReportWarning(const char module[], const char fileName[], const uint32_t lineNumber, const char className[], const IWarning* const information) {
        Core::SafeSyncType<Core::CriticalSection> guard(adminlock);
        if( _handler != nullptr ) {
            _handler->ReportWarning(module, fileName, lineNumber, className, information);
        }
    }

    bool WarningReportingUnitProxy::IsDefaultCategory(const string& module, const string& category, bool& enabled) const {
        bool retval = false;  // huppel todo -> dfit kan natuurlijk eerder komen dan de json gelezen is, dan op een of andewre manier deze ook activeren
        adminlock.Lock();
        if( _handler != nullptr ) {
            retval = _handler->IsDefaultCategory(module, category, enabled);
        }
        adminlock.Unlock();
        return retval;
    }

    void WarningReportingUnitProxy::Announce(IWarningReportingControl& Category) {
        Core::SafeSyncType<Core::CriticalSection> guard(adminlock);
        if( _handler != nullptr ) {
            _handler->Announce(Category);
        }
    }

    void WarningReportingUnitProxy::Revoke(IWarningReportingControl& Category) {
        Core::SafeSyncType<Core::CriticalSection> guard(adminlock);
        if( _handler != nullptr ) {
            _handler->Revoke(Category);
        }
    }

    void WarningReportingUnitProxy::Handler(IWarningReportingUnit* handler) {
        Core::SafeSyncType<Core::CriticalSection> guard(adminlock);
        _handler = handler;
    }

}
} 
