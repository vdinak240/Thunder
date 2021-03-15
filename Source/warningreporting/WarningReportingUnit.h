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

// ---- Include system wide include files ----

// ---- Include local include files ----
#include "IWarningReportingMedia.h"
#include "Module.h"

// ---- Helper types and constants ----

// ---- Helper functions ----
namespace WPEFramework {

namespace WarningReporting {

    constexpr uint32_t CyclicBufferSize = ((8 * 1024) - (sizeof(struct Core::CyclicBuffer::control))); /* 8Kb */
    extern EXTERNAL const TCHAR* CyclicBufferName;

    // ---- Class Definition ----
    class EXTERNAL WarningReportingUnit : public IWarningReportingUnit {
    public:
        class Setting {
        public:
            class JSON : public Core::JSON::Container {
            public:
                JSON& operator=(const JSON&) = delete;
                JSON()
                    : Core::JSON::Container()
                    , Module()
                    , Category()
                    , Enabled(false)
                {
                    Add(_T("module"), &Module); // huppel todo: module heeft waarschijnlijk geen zin
                    Add(_T("category"), &Category);
                    Add(_T("enabled"), &Enabled);
                }
                JSON(const JSON& copy)
                    : Core::JSON::Container()
                    , Module(copy.Module)
                    , Category(copy.Category)
                    , Enabled(copy.Enabled)
                {
                    Add(_T("module"), &Module);
                    Add(_T("category"), &Category);
                    Add(_T("enabled"), &Enabled);
                }
                JSON(const Setting& rhs)
                    : Core::JSON::Container()
                    , Module()
                    , Category()
                    , Enabled()
                {
                    Add(_T("module"), &Module);
                    Add(_T("category"), &Category);
                    Add(_T("enabled"), &Enabled);

                    if (rhs.HasModule()) {
                        Module = rhs.Module();
                    }
                    if (rhs.HasCategory()) {
                        Category = rhs.Category();
                    }
                    Enabled = rhs.Enabled();
                }
                virtual ~JSON()
                {
                }

            public:
                Core::JSON::String Module;
                Core::JSON::String Category;
                Core::JSON::Boolean Enabled;
            };

        public:
            Setting(const JSON& source) 
                : _module()
                , _category()
                , _enabled(source.Enabled.Value()) {
                if (source.Module.IsSet()) {
                    _module = source.Module.Value();
                }
                if (source.Category.IsSet()) {
                    _category = source.Category.Value();
                }
            }
            Setting(const Setting& copy)
                : _module(copy._module)
                , _category(copy._category)
                , _enabled(copy._enabled) {
            }
            ~Setting() {
            }

        public:
            const bool HasModule() const {
                return (_module.IsSet());
            }
            const bool HasCategory() const {
                return (_category.IsSet());
            }
            const string& Module() const {
                return (_module);
            }
            const string& Category() const {
                return (_category);
            }
            bool Enabled() const {
                return (_enabled);
            }

        private:
            Core::OptionalType<string> _module;
            Core::OptionalType<string> _category;
            bool _enabled;
        };

    public:
        typedef std::list<Setting> Settings;
        typedef std::list<IWarningReportingControl*> ControlList;
        typedef Core::IteratorType<ControlList, IWarningReportingControl*> Iterator;

    private:
        // -------------------------------------------------------------------
        // This object should not be copied or assigned. Prevent the copy
        // constructor and assignment constructor from being used. Compiler
        // generated assignment and copy methods will be blocked by the
        // following statements.
        // Define them but do not implement them, compile error/link error.
        // -------------------------------------------------------------------
        WarningReportingUnit(const WarningReportingUnit&) = delete;
        WarningReportingUnit& operator=(const WarningReportingUnit&) = delete;

        class EXTERNAL ReportingBuffer : public Core::CyclicBuffer {

        public:
            ReportingBuffer(const ReportingBuffer&) = delete;
            ReportingBuffer& operator=(const ReportingBuffer&) = delete;

            ReportingBuffer(const string& doorBell, const string& name);
            ~ReportingBuffer();

        public:
            virtual uint32_t GetOverwriteSize(Cursor& cursor) override;
            inline void Ring() {
                _doorBell.Ring();
            }
            inline void Acknowledge() {
                _doorBell.Acknowledge();
            }
            inline uint32_t Wait (const uint32_t waitTime) {
                return (_doorBell.Wait(waitTime));
            }
            inline void Relinquish()
            {
                return (_doorBell.Relinquish());
            }

        private:
            virtual void DataAvailable() override;

        private:
            Core::DoorBell _doorBell;
        };

    protected:
        WarningReportingUnit();

    public:
        ~WarningReportingUnit() override;

    public:
        static WarningReportingUnit& Instance();

        uint32_t Open(const uint32_t identifier);
        uint32_t Open(const string& pathName);
        uint32_t Close();

        void Announce(IWarningReportingControl& Category) override;
        void Revoke(IWarningReportingControl& Category) override;
        Iterator GetCategories();
        uint32_t SetCategories(const bool enable, const char* module, const char* category);

        // Default enabled/disabled categories: set via config.json.
        bool IsDefaultCategory(const string& module, const string& category, bool& enabled) const override;
        string Defaults() const;
        void Defaults(const string& jsonCategories);
        void Defaults(Core::File& file);

        void ReportWarning(const char module[], const char fileName[], const uint32_t lineNumber, const char className[], const IWarning* const information) override;

        inline Core::CyclicBuffer* CyclicBuffer()
        {
            return (m_OutputChannel);
        }
        inline bool HasDirectOutput() const
        {
            return (m_DirectOut);
        }
        inline void DirectOutput(const bool enabled)
        {
            m_DirectOut = enabled;
        }
        inline void Announce() {
            ASSERT (m_OutputChannel != nullptr);
            m_OutputChannel->Ring();
        }
        inline void Acknowledge() {
            ASSERT (m_OutputChannel != nullptr);
            m_OutputChannel->Acknowledge();
        }
        inline uint32_t Wait (const uint32_t waitTime) {
            ASSERT (m_OutputChannel != nullptr);
            return (m_OutputChannel->Wait(waitTime));
        }
        inline void Relinquish() {
            ASSERT(m_OutputChannel != nullptr);
            return (m_OutputChannel->Relinquish());
        }

    private:
        inline uint32_t Open(const string& doorBell, const string& fileName) 
        {

            /*  huppel todo: onoy report localy for testing purposes

            ASSERT(m_OutputChannel == nullptr);

            m_OutputChannel = new ReportingBuffer(doorBell, fileName);

            ASSERT(m_OutputChannel->IsValid() == true);

            return (m_OutputChannel->IsValid() ? Core::ERROR_NONE : Core::ERROR_UNAVAILABLE);

            */
            DirectOutput(true);
            return Core::ERROR_NONE;

        }
        void UpdateEnabledCategories(const Core::JSON::ArrayType<Setting::JSON>& info);

        ControlList m_Categories;
        Core::CriticalSection m_Admin;
        ReportingBuffer* m_OutputChannel;
        Settings m_EnabledCategories;
        bool m_DirectOut;
    };
}
} 
