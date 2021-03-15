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

#include <inttypes.h> 

#include "IWarningReportingControl.h"
#include "TextFragment.h"
#include "Trace.h"
#include "SystemInfo.h"
#include "Time.h"
#include "Module.h"

#ifndef WARNING_REPORTING

#define REPORT_WARNING_THREAD_SETCALLSIGN(CALLSIGN)

#define REPORT_WARNING(CATEGORY, PARAMETERS)                                                  

#define REPORT_WARNING_GLOBAL(CATEGORY, PARAMETERS)                             

#define REPORT_DURATION_WARNING(CODE, CATEGORY, PARAMETERS)                                 \
    CODE

#else

#define REPORT_WARNING_THREAD_SETCALLSIGN(CALLSIGN)    \
    WPEFramework::WarningReporting::CallsignTLS::CallSignTLSGuard callsignguard(CALLSIGN); 

// ---- Helper types and constants ----
#define REPORT_WARNING(CATEGORY, PARAMETERS)                                                    \
    if (WPEFramework::WarningReporting::WarningReportingType<CATEGORY, &WPEFramework::Core::System::MODULE_NAME>::IsEnabled() == true) { \
        CATEGORY __data__ PARAMETERS;                                                  \
        WPEFramework::WarningReporting::WarningReportingType<CATEGORY, &WPEFramework::Core::System::MODULE_NAME> __message__(__data__);  \
        WPEFramework::WarningReporting::WarningReportingUnitProxy::Instance().ReportWarning(                                            \
            __FILE__,                                                                  \
            __LINE__,                                                                  \
            typeid(*this).name(),                                                      \
            &__message__);                                                             \
    }

#define REPORT_WARNING_GLOBAL(CATEGORY, PARAMETERS)                                             \
    if (WPEFramework::WarningReporting::WarningReportingType<CATEGORY, &WPEFramework::Core::System::MODULE_NAME>::IsEnabled() == true) { \
        CATEGORY __data__ PARAMETERS;                                                  \
        WPEFramework::WarningReporting::WarningReportingType<CATEGORY, &WPEFramework::Core::System::MODULE_NAME> __message__(__data__);  \
        WPEFramework::WarningReporting::WarningReportingUnitProxy::Instance().ReportWarning(                                            \
            __FILE__,                                                                  \
            __LINE__,                                                                  \
            __FUNCTION__,                                                              \
            &__message__);                                                             \
    }

#define REPORT_DURATION_WARNING(CODE, CATEGORY, PARAMETERS)                                 \
    if (WPEFramework::WarningReporting::WarningReportingType<CATEGORY, &WPEFramework::Core::System::MODULE_NAME>::IsEnabled() == true) { \
        WPEFramework::Core::Time start = WPEFramework::Core::Time::Now();                     \
        CODE                                                                            \
        uint64_t duration( (Core::Time::Now().Ticks() - start.Ticks()) / Core::Time::TicksPerMillisecond);              \
        if( duration > CATEGORY::MaxAllowedDurationMs() ) {                           \
            CATEGORY __data__ (duration, PARAMETERS);                                               \
            WPEFramework::WarningReporting::WarningReportingType<CATEGORY, &WPEFramework::Core::System::MODULE_NAME> __message__(__data__);  \
            WPEFramework::WarningReporting::WarningReportingUnitProxy::Instance().ReportWarning(                                            \
            WPEFramework::WarningReporting::CallsignTLS::CallsignAccess<&WPEFramework::Core::System::MODULE_NAME>::Callsign(),                                               \
            __FILE__,                                                                  \
            __LINE__,                                                                  \
            typeid(*this).name(),                                                      \
            &__message__);                                                             \
        }                                                                              \
    } else {                                                                           \
        CODE                                                                           \
    }


namespace WPEFramework {

namespace Core {
    template <typename THREADLOCALSTORAGE>
    class ThreadLocalStorageType;
}

namespace WarningReporting {

    class CallsignTLS {
    public:

        template <const char** MODULENAME>
        struct CallsignAccess {
            static const char* Callsign() {
                const char* callsign = CallsignTLS::Callsign();
                if( callsign == nullptr ) {
                    callsign = *MODULENAME;
                }
                return callsign;
            }
        };

        class CallSignTLSGuard {
        public:
            CallSignTLSGuard(const CallSignTLSGuard&) = delete;
            CallSignTLSGuard& operator=(const CallSignTLSGuard&) = delete;

            explicit CallSignTLSGuard(const char* callsign) {
                CallsignTLS::Callsign(callsign);
            }

            ~CallSignTLSGuard() {
                CallsignTLS::Callsign(nullptr);
            }

        };

        static const char* Callsign();
        static void Callsign(const char* callsign);

    private:
        friend class Core::ThreadLocalStorageType<CallsignTLS>;
    
        CallsignTLS(const CallsignTLS&) = delete;
        CallsignTLS& operator=(const CallsignTLS&) = delete;

        CallsignTLS() : _name() {};
        ~CallsignTLS() = default;

        void Name(const char* name) {  
            if ( name != nullptr ) {
                _name = name; 
            } else {
                _name.clear(); 
            }
        }
        const char* Name() const { 
            return ( _name.empty() == false ? _name.c_str() : nullptr ); 
        }

    private:
        string _name;
    };

    class WarningReportingUnitProxy {
    public:
        WarningReportingUnitProxy(const WarningReportingUnitProxy&) = delete;
        WarningReportingUnitProxy& operator=(const WarningReportingUnitProxy&) = delete;

        ~WarningReportingUnitProxy() = default;

        static WarningReportingUnitProxy& Instance();

        void ReportWarning(const char module[], const char fileName[], const uint32_t lineNumber, const char className[], const IWarning* const information);
        bool IsDefaultCategory(const string& module, const string& category, bool& enabled) const;
        void Announce(IWarningReportingControl& Category);
        void Revoke(IWarningReportingControl& Category);

        void Handler(IWarningReportingUnit* handler);

    protected:
        WarningReportingUnitProxy() : _handler(nullptr) {};

    private:
       IWarningReportingUnit* _handler; 
    };

    template <typename CATEGORY, const char** MODULENAME>
    class WarningReportingType : public IWarning {
    private:
        template <typename CONTROLCATEGORY, const char** CONTROLMODULE>
        class WarningReportingControl : public IWarningReportingControl {
        private:

        public:
            WarningReportingControl(const WarningReportingControl<CONTROLCATEGORY, CONTROLMODULE>&) = delete;
            WarningReportingControl<CONTROLCATEGORY, CONTROLMODULE>& operator=(const WarningReportingControl<CONTROLCATEGORY, CONTROLMODULE>&) = delete;

            WarningReportingControl()
                : m_CategoryName(Core::ClassNameOnly(typeid(CONTROLCATEGORY).name()).Text())
                , m_Enabled(0x03) // huppel todo: for now turn on by default
            {
                // Register Our control unit, so it can be influenced from the outside
                // if nessecary..
                WarningReportingUnitProxy::Instance().Announce(*this);

                bool enabled = false;
                if (WarningReportingUnitProxy::Instance().IsDefaultCategory(*CONTROLMODULE, m_CategoryName, enabled)) {
                    if (enabled) {
                        // Better not to use virtual Enabled(...), because derived classes aren't finished yet.
                        m_Enabled = m_Enabled | 0x01;
                    }
                }
            }
            ~WarningReportingControl() override
            {
                Destroy();
            }

        public:
            inline bool IsEnabled() const
            {
                return ((m_Enabled & 0x01) != 0);
            }
            virtual const char* Category() const
            {
                return (m_CategoryName.c_str());
            }
            virtual const char* Module() const
            {
                return (*CONTROLMODULE);
            }
            virtual bool Enabled() const
            {
                return (IsEnabled());
            }
            virtual void Enabled(const bool enabled)
            {
                m_Enabled = (m_Enabled & 0xFE) | (enabled ? 0x01 : 0x00);
            }
            virtual void Destroy()
            {
                if ((m_Enabled & 0x02) != 0) {
                    WarningReportingUnitProxy::Instance().Revoke(*this);
                    m_Enabled = 0;
                }
            }

        protected:
            const string m_CategoryName;
            uint8_t m_Enabled;
        };


    public:
        WarningReportingType(const WarningReportingType<CATEGORY, MODULENAME>&) = delete;
        WarningReportingType<CATEGORY, MODULENAME>& operator=(const WarningReportingType<CATEGORY, MODULENAME>&) = delete;

        WarningReportingType(CATEGORY& category)
            : _info(category)
        {
        }
        virtual ~WarningReportingType() = default;

    public:
        // No dereference etc.. 1 straight line to enabled or not... Quick method..
        inline static bool IsEnabled()
        {
            return (s_control.IsEnabled());
        }

        inline static void Enable(const bool status)
        {
            s_control.Enabled(status);
        }

        virtual const char* Category() const
        {
            return (s_control.Category());
        }

        virtual const char* Module() const
        {
            return (s_control.Module());
        }

        virtual bool Enabled() const
        {
            return (s_control.Enabled());
        }

        virtual void Enabled(const bool enabled)
        {
            s_control.Enabled(enabled);
        }

        virtual void Destroy()
        {
            s_control.Destroy();
        }

        virtual const char* Data() const
        {
            return (_info.Data());
        }
        virtual uint16_t Length() const
        {
            return (_info.Length());
        }

    private:
        CATEGORY& _info;
        static WarningReportingControl<CATEGORY, MODULENAME> s_control;
    };

    template <typename CATEGORY, const char** MODULENAME>
    EXTERNAL_HIDDEN typename WarningReportingType<CATEGORY, MODULENAME>::template WarningReportingControl<CATEGORY, MODULENAME> WarningReportingType<CATEGORY, MODULENAME>::s_control;
}

} 

#endif

