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

#include "WarningReportingUnit.h"

#define WARNINGREPORTING_CYCLIC_BUFFER_FILENAME _T("WARNINGREPORTING_FILENAME")
#define WARNINGREPORTING_CYCLIC_BUFFER_DOORBELL _T("WARNINGREPORTING_DOORBELL")

namespace WPEFramework {
namespace WarningReporting {

    /* static */ const TCHAR* CyclicBufferName = _T("warningreportingbuffer");

    WarningReportingUnit::WarningReportingUnit()
        : m_Categories()
        , m_Admin()
        , m_OutputChannel(nullptr)
        , m_DirectOut(false)
    {
        WarningReportingUnitProxy::Instance().Handler(this);
    }

    WarningReportingUnit::ReportingBuffer::ReportingBuffer(const string& doorBell, const string& name)
        : Core::CyclicBuffer(name, 
                                Core::File::USER_READ    | 
                                Core::File::USER_WRITE   | 
                                Core::File::USER_EXECUTE | 
                                Core::File::GROUP_READ   |
                                Core::File::GROUP_WRITE  |
                                Core::File::OTHERS_READ  |
                                Core::File::OTHERS_WRITE | 
                                Core::File::SHAREABLE,
                             CyclicBufferSize, true)
        , _doorBell(doorBell.c_str())
    {
        // Make sure the trace file opened proeprly.
        TRACE_L1("Opened a file to stash my reported warning at: %s [%d] and doorbell: %s", name.c_str(), CyclicBufferSize, doorBell.c_str());
        ASSERT (IsValid() == true);
    }

    WarningReportingUnit::ReportingBuffer::~ReportingBuffer()
    {
    }

    /* virtual */ uint32_t WarningReportingUnit::ReportingBuffer::GetOverwriteSize(Cursor& cursor)
    {
        while (cursor.Offset() < cursor.Size()) {
            uint16_t chunkSize = 0;
            cursor.Peek(chunkSize);

            TRACE_L1("Flushing warning reporting data !!! %d", __LINE__);

            cursor.Forward(chunkSize);
        }

        return cursor.Offset();
    }

    /* virtual */ void WarningReportingUnit::ReportingBuffer::DataAvailable()
    {
        _doorBell.Ring();
    }

    /* static */ WarningReportingUnit& WarningReportingUnit::Instance()
    {
        return (Core::SingletonType<WarningReportingUnit>::Instance());
    }

    WarningReportingUnit::~WarningReportingUnit()
    {

        WarningReportingUnitProxy::Instance().Handler(nullptr);

        m_Admin.Lock();

        if (m_OutputChannel != nullptr) {
            Close();
        }

        while (m_Categories.size() != 0) {
            m_Categories.front()->Destroy();
        }

        m_Admin.Unlock();
    }

    uint32_t WarningReportingUnit::Open(const uint32_t identifier)
    {
        uint32_t result = Core::ERROR_UNAVAILABLE;

        string fileName;
        string doorBell;
        Core::SystemInfo::GetEnvironment(WARNINGREPORTING_CYCLIC_BUFFER_FILENAME, fileName);
        Core::SystemInfo::GetEnvironment(WARNINGREPORTING_CYCLIC_BUFFER_DOORBELL, doorBell);

        ASSERT(fileName.empty() == false);
        ASSERT(doorBell.empty() == false);

        if (fileName.empty() == false) {
       
            fileName +=  '.' + Core::NumberType<uint32_t>(identifier).Text();
            result = Open(doorBell, fileName);
        }

        return (result);
    }

    uint32_t WarningReportingUnit::Open(const string& pathName)
    {
        string fileName(Core::Directory::Normalize(pathName) + CyclicBufferName);
        #ifdef __WINDOWS__
        string doorBell("127.0.0.1:62002"); // huppel todo: is deze vrij?
        #else
        string doorBell(Core::Directory::Normalize(pathName) + CyclicBufferName + ".doorbell" );
        #endif

        Core::SystemInfo::SetEnvironment(WARNINGREPORTING_CYCLIC_BUFFER_FILENAME, fileName);
        Core::SystemInfo::SetEnvironment(WARNINGREPORTING_CYCLIC_BUFFER_DOORBELL, doorBell);

        return (Open(doorBell, fileName));
    }

    uint32_t WarningReportingUnit::Close()
    {
        m_Admin.Lock();

        ASSERT(m_OutputChannel != nullptr);

        if (m_OutputChannel != nullptr) {
            delete m_OutputChannel;
        }

        m_OutputChannel = nullptr;

        m_Admin.Unlock();

        return (Core::ERROR_NONE);
    }

    void WarningReportingUnit::Announce(IWarningReportingControl& Category)
    {
        m_Admin.Lock();

        m_Categories.push_back(&Category);

        m_Admin.Unlock();
    }

    void WarningReportingUnit::Revoke(IWarningReportingControl& Category)
    {
        m_Admin.Lock();

        std::list<IWarningReportingControl*>::iterator index(std::find(m_Categories.begin(), m_Categories.end(), &Category));

        if (index != m_Categories.end()) {
            m_Categories.erase(index);
        }

        m_Admin.Unlock();
    }

    WarningReportingUnit::Iterator WarningReportingUnit::GetCategories()
    {
        return (Iterator(m_Categories));
    }

    uint32_t WarningReportingUnit::SetCategories(const bool enable, const char* module, const char* category)
    {
        uint32_t modifications = 0;

        ControlList::iterator index(m_Categories.begin());

        while (index != m_Categories.end()) {
            const char* thisModule = (*index)->Module();
            const char* thisCategory = (*index)->Category();

            if ((module != nullptr) && (category != nullptr)) {
                if ((::strcmp(module, thisModule) == 0) && (::strcmp(category, thisCategory) == 0)) {
                    modifications++;
                    (*index)->Enabled(enable);
                }
            }
            else if ((module != nullptr) && (category == nullptr)) {
                if ((::strcmp(module, thisModule) == 0)) {
                    //Disable/Enable traces for selected module
                    modifications++;
                    (*index)->Enabled(enable);
                }
            }
            else {
                //Disable/Enable traces for all modules
                modifications++;
                (*index)->Enabled(enable);
            }

            index++;
        }

        return (modifications);
    }

    string WarningReportingUnit::Defaults() const
    {
        string result;
        Core::JSON::ArrayType<Setting::JSON> serialized;
        Settings::const_iterator index = m_EnabledCategories.begin();
        
        while (index != m_EnabledCategories.end()) {
            serialized.Add(Setting::JSON(*index));
            index++;
        }

        serialized.ToString(result);
        return (result);
    }

    void WarningReportingUnit::Defaults(const string& jsonCategories)
    {
        Core::JSON::ArrayType<Setting::JSON> serialized;
        Core::OptionalType<Core::JSON::Error> error;
        serialized.FromString(jsonCategories, error);
        if (error.IsSet() == true) {
            SYSLOG(Logging::ParsingError, (_T("Parsing failed with %s"), ErrorDisplayMessage(error.Value()).c_str()));
        }

        // Deal with existing categories that might need to be enable/disabled.
        UpdateEnabledCategories(serialized);
    }

    void WarningReportingUnit::Defaults(Core::File& file) {
        Core::JSON::ArrayType<Setting::JSON> serialized;
        Core::OptionalType<Core::JSON::Error> error;
        serialized.IElement::FromFile(file, error);
        if (error.IsSet() == true) {
            SYSLOG(WPEFramework::Logging::ParsingError, (_T("Parsing failed with %s"), ErrorDisplayMessage(error.Value()).c_str()));
        }

        // Deal with existing categories that might need to be enable/disabled.
        UpdateEnabledCategories(serialized);
    }

    void WarningReportingUnit::UpdateEnabledCategories(const Core::JSON::ArrayType<Setting::JSON>& info)
    {
        Core::JSON::ArrayType<Setting::JSON>::ConstIterator index = info.Elements();

        m_EnabledCategories.clear();

        while (index.Next()) {
            m_EnabledCategories.emplace_back(Setting(index.Current()));
        }

        for (IWarningReportingControl* traceControl : m_Categories) {
            Settings::const_iterator index = m_EnabledCategories.begin();
            while (index != m_EnabledCategories.end()) {
                const Setting& setting = *index;

                if ( ((setting.HasModule()   == false) || (setting.Module()   == traceControl->Module())   ) && 
                     ((setting.HasCategory() == false) || (setting.Category() == traceControl->Category()) ) ) {
                    if (setting.Enabled() != traceControl->Enabled()) {
                        traceControl->Enabled(setting.Enabled());
                    }
                }

                index++;
            }
        }
    }

    bool WarningReportingUnit::IsDefaultCategory(const string& module, const string& category, bool& enabled) const
    {
        bool isDefaultCategory = false;

        Settings::const_iterator index = m_EnabledCategories.begin();
        while (index != m_EnabledCategories.end()) {
            const Setting& setting = *index;

            if ( ((module.empty() == true)   || (setting.HasModule()   == false) || (setting.Module()   == module)   ) && 
                 ((category.empty() == true) || (setting.HasCategory() == false) || (setting.Category() == category) ) ) {
                // Register match of category and update enabled flag.
                isDefaultCategory = true;
                enabled = setting.Enabled();
            }
            index++;
        }

        return isDefaultCategory;
    }

    void WarningReportingUnit::ReportWarning(const char module[], const char file[], const uint32_t lineNumber, const char className[], const IWarning* const information)
    {

        // huppel todo: module is now a hack, not put on the buffer, how do we deal with that and the 'real' module

        const char* fileName(Core::FileNameOnly(file));

        m_Admin.Lock();

        if (m_OutputChannel != nullptr) {

            const char* category(information->Category());
            const char* module(information->Module());
            const uint64_t current = Core::Time::Now().Ticks();
            const uint16_t fileNameLength = static_cast<uint16_t>(strlen(fileName) + 1); // File name.
            const uint16_t moduleLength = static_cast<uint16_t>(strlen(module) + 1); // Module.
            const uint16_t categoryLength = static_cast<uint16_t>(strlen(category) + 1); // Cateogory.
            const uint16_t classNameLength = static_cast<uint16_t>(strlen(className) + 1); // Class name.
            const uint16_t informationLength = information->Length(); // Actual data (no '\0' needed).

            // Trace entry has been simplified: 16 bit size followed by fields:
            // length(2 bytes) - clock ticks (8 bytes) - line number (4 bytes) - file/module/category/className
            const uint16_t headerLength = 2 + 8 + 4 + fileNameLength + moduleLength + categoryLength + classNameLength;

            const uint32_t fullLength = informationLength + headerLength; // Actual data (no '\0' needed).

            // Tell the buffer how much we are going to write.
            const uint32_t actualLength = m_OutputChannel->Reserve(fullLength);

            if (actualLength >= headerLength) {
                const uint16_t convertedLength = static_cast<uint16_t>(actualLength);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(&convertedLength), 2);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(&current), 8);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(&lineNumber), 4);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(fileName), fileNameLength);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(module), moduleLength);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(category), categoryLength);
                m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(className), classNameLength);

                if (actualLength >= fullLength) {
                    // We can write the whole information.
                    m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(information->Data()), informationLength);
                } else {
                    // Can only write information partially
                    const uint16_t dropLength = actualLength - headerLength;

                    m_OutputChannel->Write(reinterpret_cast<const uint8_t*>(information->Data()), dropLength);
                }
            }
        }

        if (m_DirectOut == true) {
            string time(Core::Time::Now().ToRFC1123(true));
            Core::TextFragment cleanClassName(Core::ClassNameOnly(className));

            fprintf(stdout, "[%s]:[%s:%s][%s:%d]:[%s] %s: %s\n", time.c_str(), module, information->Module(), fileName, lineNumber, cleanClassName.Data(), information->Category(), information->Data());
            fflush(stdout);
        }

        m_Admin.Unlock();
    }
}
} 
