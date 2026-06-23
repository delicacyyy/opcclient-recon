#include "commands.h"
#include "opc_client.h"

namespace opcclient {
namespace {

struct ServerCandidate {
    CLSID clsid{};
    std::wstring progId;
    std::wstring userType;
    std::set<std::wstring> categories;
};

struct BrowseEntry {
    bool branch = false;
    std::wstring name;
    std::wstring itemId;
};

std::vector<ServerCandidate> EnumerateServerCandidates(OpcSession& session) {
    struct CategoryInfo {
        const CATID* id;
        const wchar_t* label;
    };
    const CategoryInfo categories[] = {
        {&CATID_OPCDAServer30, L"DA 3.0"},
        {&CATID_OPCDAServer20, L"DA 2.0"},
        {&CATID_OPCDAServer10, L"DA 1.0"}
    };

    session.ConnectServerList();
    std::map<std::wstring, ServerCandidate> byClsid;
    for (const CategoryInfo& category : categories) {
        CATID id = *category.id;
        ComPtr<IEnumGUID> enumerator;
        HRESULT hr = session.ServerList()->EnumClassesOfCategories(
            1,
            &id,
            0,
            nullptr,
            enumerator.Put()
        );
        if (FAILED(hr)) {
            std::wcout << L"discovery.category=" << category.label
                       << L" error=" << HResultText(hr) << L"\n";
            continue;
        }
        Check(session.Blanket(enumerator.Get()), L"CoSetProxyBlanket(server enumerator)");

        while (true) {
            CLSID clsid{};
            ULONG fetched = 0;
            hr = enumerator->Next(1, &clsid, &fetched);
            if (hr == S_FALSE || fetched == 0) {
                break;
            }
            Check(hr, L"IEnumGUID::Next");
            std::wstring key = GuidText(clsid);
            auto [iterator, inserted] = byClsid.try_emplace(key);
            ServerCandidate& candidate = iterator->second;
            if (inserted) {
                candidate.clsid = clsid;
                LPOLESTR progId = nullptr;
                LPOLESTR userType = nullptr;
                HRESULT details = session.ServerList()->GetClassDetails(
                    clsid,
                    &progId,
                    &userType
                );
                if (SUCCEEDED(details)) {
                    candidate.progId = progId ? progId : L"";
                    candidate.userType = userType ? userType : L"";
                } else {
                    candidate.progId = L"(details unavailable)";
                    candidate.userType = HResultText(details);
                }
                CoTaskMemFree(progId);
                CoTaskMemFree(userType);
            }
            candidate.categories.insert(category.label);
        }
    }

    std::vector<ServerCandidate> result;
    for (auto& [key, candidate] : byClsid) {
        result.push_back(std::move(candidate));
    }
    std::sort(
        result.begin(),
        result.end(),
        [](const ServerCandidate& left, const ServerCandidate& right) {
            return left.progId < right.progId;
        }
    );
    return result;
}

int PromptIndex(std::size_t count, const wchar_t* prompt, bool allowBack = false) {
    while (true) {
        std::wcout << prompt;
        std::wstring input;
        if (!std::getline(std::wcin, input)) {
            return -1;
        }
        input = Trim(input);
        if (input == L"q" || input == L"Q") {
            return -1;
        }
        if (allowBack && (input == L"b" || input == L"B")) {
            return -2;
        }
        try {
            std::size_t consumed = 0;
            unsigned long selected = std::stoul(input, &consumed);
            if (consumed == input.size() && selected >= 1 && selected <= count) {
                return static_cast<int>(selected - 1);
            }
        } catch (...) {
        }
        std::wcout << L"Enter a number from 1 to " << count;
        if (allowBack) {
            std::wcout << L", b to go back";
        }
        std::wcout << L", or q to quit.\n";
    }
}

std::vector<std::wstring> EnumerateBrowseNames(
    IOPCBrowseServerAddressSpace* browser,
    OPCBROWSETYPE type,
    DWORD accessRights,
    const ConnectionConfig* config
) {
    ComPtr<IEnumString> enumerator;
    Check(
        browser->BrowseOPCItemIDs(type, L"", VT_EMPTY, accessRights, enumerator.Put()),
        L"IOPCBrowseServerAddressSpace::BrowseOPCItemIDs"
    );
    Check(ApplyProxyBlanket(enumerator.Get(), config), L"CoSetProxyBlanket(item enumerator)");

    std::vector<std::wstring> names;
    while (true) {
        LPOLESTR value = nullptr;
        ULONG fetched = 0;
        HRESULT hr = enumerator->Next(1, &value, &fetched);
        if (hr == S_FALSE || fetched == 0) {
            break;
        }
        Check(hr, L"IEnumString::Next");
        names.emplace_back(value ? value : L"");
        CoTaskMemFree(value);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::wstring BrowseInteractive(OpcSession& session) {
    ComPtr<IOPCBrowseServerAddressSpace> browser;
    Check(
        QueryRemoteInterface(
            session.Server(),
            IID_IOPCBrowseServerAddressSpace,
            session.Config(),
            browser.Put()
        ),
        L"QueryInterface(IOPCBrowseServerAddressSpace)"
    );

    OPCNAMESPACETYPE namespaceType{};
    Check(browser->QueryOrganization(&namespaceType), L"IOPCBrowseServerAddressSpace::QueryOrganization");
    const std::size_t pageSize = 20;
    std::vector<std::wstring> path;

    while (true) {
        std::vector<BrowseEntry> entries;
        if (namespaceType == OPC_NS_HIERARCHIAL) {
            for (const std::wstring& branch :
                 EnumerateBrowseNames(browser.Get(), OPC_BRANCH, 0, session.Config())) {
                entries.push_back({true, branch, L""});
            }
        }

        OPCBROWSETYPE leafType = namespaceType == OPC_NS_HIERARCHIAL ? OPC_LEAF : OPC_FLAT;
        for (const std::wstring& leaf :
             EnumerateBrowseNames(browser.Get(), leafType, OPC_READABLE, session.Config())) {
            LPWSTR fullItemId = nullptr;
            HRESULT itemHr = namespaceType == OPC_NS_HIERARCHIAL
                ? browser->GetItemID(const_cast<LPWSTR>(leaf.c_str()), &fullItemId)
                : S_OK;
            std::wstring itemId;
            if (namespaceType == OPC_NS_HIERARCHIAL) {
                if (FAILED(itemHr)) {
                    continue;
                }
                itemId = fullItemId ? fullItemId : L"";
                CoTaskMemFree(fullItemId);
            } else {
                itemId = leaf;
            }
            entries.push_back({false, leaf, itemId});
        }

        std::size_t page = 0;
        while (true) {
            std::wcout << L"\nNamespace: /";
            for (const std::wstring& component : path) {
                std::wcout << component << L"/";
            }
            std::wcout << L"\n";

            if (entries.empty()) {
                std::wcout << L"No readable items or child branches were returned.\n";
            }
            std::size_t begin = page * pageSize;
            std::size_t end = std::min(entries.size(), begin + pageSize);
            for (std::size_t i = begin; i < end; ++i) {
                std::wcout << (i + 1) << L") "
                           << (entries[i].branch ? L"[branch] " : L"[item] ")
                           << entries[i].name << L"\n";
            }
            std::wcout << L"Commands: item number";
            if (end < entries.size()) {
                std::wcout << L", n=next";
            }
            if (page > 0) {
                std::wcout << L", p=previous";
            }
            std::wcout << L", b=back, q=server menu\n> ";

            std::wstring input;
            if (!std::getline(std::wcin, input)) {
                return {};
            }
            input = Trim(input);
            if (input == L"q" || input == L"Q") {
                return {};
            }
            if (input == L"n" || input == L"N") {
                if (end < entries.size()) {
                    ++page;
                }
                continue;
            }
            if (input == L"p" || input == L"P") {
                if (page > 0) {
                    --page;
                }
                continue;
            }
            if (input == L"b" || input == L"B") {
                if (namespaceType == OPC_NS_HIERARCHIAL && !path.empty()) {
                    Check(
                        browser->ChangeBrowsePosition(OPC_BROWSE_UP, L""),
                        L"IOPCBrowseServerAddressSpace::ChangeBrowsePosition(up)"
                    );
                    path.pop_back();
                    break;
                }
                return {};
            }

            try {
                std::size_t consumed = 0;
                unsigned long selected = std::stoul(input, &consumed);
                if (consumed != input.size() || selected < 1 || selected > entries.size()) {
                    throw std::out_of_range("selection");
                }
                BrowseEntry& entry = entries[selected - 1];
                if (entry.branch) {
                    Check(
                        browser->ChangeBrowsePosition(OPC_BROWSE_DOWN, entry.name.c_str()),
                        L"IOPCBrowseServerAddressSpace::ChangeBrowsePosition(down)"
                    );
                    path.push_back(entry.name);
                    break;
                }
                return entry.itemId;
            } catch (...) {
                std::wcout << L"Invalid selection.\n";
            }
        }
    }
}

void PrintInteractiveStatus(OpcSession& session, const ServerCandidate& candidate) {
    OPCSERVERSTATUS* status = nullptr;
    Check(session.Server()->GetStatus(&status), L"IOPCServer::GetStatus");
    std::wcout << L"\nSelected server\n"
               << L"  ProgID: " << candidate.progId << L"\n"
               << L"  User type: " << candidate.userType << L"\n"
               << L"  CLSID: " << GuidText(candidate.clsid) << L"\n"
               << L"  State: " << ServerStateText(status->dwServerState) << L"\n"
               << L"  Vendor: " << (status->szVendorInfo ? status->szVendorInfo : L"") << L"\n"
               << L"  Version: " << status->wMajorVersion << L"." << status->wMinorVersion
               << L"." << status->wBuildNumber << L"\n";
    CoTaskMemFree(status->szVendorInfo);
    CoTaskMemFree(status);

    struct InterfaceInfo {
        const IID* iid;
        const wchar_t* name;
    };
    const InterfaceInfo interfaces[] = {
        {&IID_IOPCBrowseServerAddressSpace, L"IOPCBrowseServerAddressSpace"},
        {&IID_IOPCItemProperties, L"IOPCItemProperties"},
        {&IID_IOPCCommon, L"IOPCCommon"}
    };
    for (const InterfaceInfo& info : interfaces) {
        ComPtr<IUnknown> queried;
        HRESULT hr = QueryRemoteInterface(
            session.Server(),
            *info.iid,
            session.Config(),
            queried.Put()
        );
        std::wcout << L"  Interface " << info.name << L": "
                   << (SUCCEEDED(hr) ? L"supported" : HResultText(hr)) << L"\n";
    }
}

void PerformInteractiveSyncRead(OpcSession& session, const std::wstring& itemId) {
    PrintItemProperties(session.Server(), itemId, session.Config());
    OpcGroup group(session.Server(), itemId, session.Config());
    ComPtr<IOPCSyncIO> sync;
    Check(group.GetSync(sync), L"QueryInterface(IOPCSyncIO)");
    OPCHANDLE handle = group.ServerItemHandle();
    OPCITEMSTATE* values = nullptr;
    HRESULT* errors = nullptr;
    Check(sync->Read(OPC_DS_DEVICE, 1, &handle, &values, &errors), L"IOPCSyncIO::Read");
    PrintSyncRead(values[0], errors[0]);
    VariantClear(&values[0].vDataValue);
    CoTaskMemFree(values);
    CoTaskMemFree(errors);
}

void PerformInteractiveAsyncRead(OpcSession& session, const std::wstring& itemId) {
    OpcGroup group(session.Server(), itemId, session.Config());
    auto* callback = new OpcCallback();
    try {
        Check(group.Advise(callback), L"Advise(IOPCDataCallback)");
        ComPtr<IOPCAsyncIO2> async;
        Check(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");
        Check(async->SetEnable(TRUE), L"IOPCAsyncIO2::SetEnable");
        callback->Reset();
        OPCHANDLE handle = group.ServerItemHandle();
        DWORD cancelId = 0;
        HRESULT* errors = nullptr;
        constexpr DWORD transactionId = 1001;
        Check(async->Read(1, &handle, transactionId, &cancelId, &errors), L"IOPCAsyncIO2::Read");
        HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
        CoTaskMemFree(errors);
        Check(itemError, L"Asynchronous item read");
        if (callback->Wait(15000) != WAIT_OBJECT_0 || !callback->ReadCompleted()) {
            throw std::runtime_error("Timed out waiting for OnReadComplete");
        }
    } catch (...) {
        callback->Release();
        throw;
    }
    callback->Release();
}

void PerformInteractiveSubscription(
    OpcSession& session,
    const std::wstring& itemId,
    int seconds
) {
    OpcGroup group(session.Server(), itemId, session.Config(), 1000);
    auto* callback = new OpcCallback();
    try {
        Check(group.Advise(callback), L"Advise(IOPCDataCallback)");
        ComPtr<IOPCAsyncIO2> async;
        Check(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");
        Check(async->SetEnable(TRUE), L"IOPCAsyncIO2::SetEnable");
        std::wcout << L"Observing subscription for " << seconds << L" seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        std::wcout << L"subscription.data_change_items=" << callback->DataChanges() << L"\n";
    } catch (...) {
        callback->Release();
        throw;
    }
    callback->Release();
}

} // namespace

void Interactive(const Options&, const RuntimeSettings& settings) {
    OpcSession discovery(settings.host, settings.config.get());
    std::vector<ServerCandidate> servers = EnumerateServerCandidates(discovery);
    if (servers.empty()) {
        throw std::runtime_error("OPCEnum returned no OPC DA 1.0, 2.0, or 3.0 servers");
    }

    while (true) {
        std::wcout << L"\nDiscovered OPC DA servers on " << settings.host << L"\n";
        for (std::size_t i = 0; i < servers.size(); ++i) {
            std::wcout << (i + 1) << L") " << servers[i].progId
                       << L" — " << servers[i].userType
                       << L" — " << GuidText(servers[i].clsid)
                       << L" — ";
            bool first = true;
            for (const std::wstring& category : servers[i].categories) {
                if (!first) {
                    std::wcout << L", ";
                }
                std::wcout << category;
                first = false;
            }
            std::wcout << L"\n";
        }

        int serverIndex = PromptIndex(servers.size(), L"Select server (q=quit): ");
        if (serverIndex < 0) {
            return;
        }
        const ServerCandidate& candidate = servers[static_cast<std::size_t>(serverIndex)];
        OpcSession session(settings.host, settings.config.get());
        session.Connect(candidate.clsid, candidate.progId);
        PrintInteractiveStatus(session, candidate);

        bool chooseAnotherServer = false;
        while (!chooseAnotherServer) {
            std::wstring itemId = BrowseInteractive(session);
            if (itemId.empty()) {
                break;
            }
            std::wcout << L"\nSelected item: " << itemId << L"\n";
            PrintItemProperties(session.Server(), itemId, session.Config());

            bool chooseAnotherItem = false;
            while (!chooseAnotherItem && !chooseAnotherServer) {
                std::wcout <<
                    L"\n1) Synchronous read\n"
                    L"2) Asynchronous read\n"
                    L"3) Timed subscription\n"
                    L"4) Choose another item\n"
                    L"5) Choose another server\n"
                    L"q) Exit\n> ";
                std::wstring action;
                if (!std::getline(std::wcin, action)) {
                    return;
                }
                action = Trim(action);
                if (action == L"1") {
                    PerformInteractiveSyncRead(session, itemId);
                } else if (action == L"2") {
                    PerformInteractiveAsyncRead(session, itemId);
                } else if (action == L"3") {
                    std::wcout << L"Subscription duration in seconds (1-3600): ";
                    std::wstring duration;
                    std::getline(std::wcin, duration);
                    try {
                        int seconds = std::stoi(Trim(duration));
                        if (seconds < 1 || seconds > 3600) {
                            throw std::out_of_range("seconds");
                        }
                        PerformInteractiveSubscription(session, itemId, seconds);
                    } catch (...) {
                        std::wcout << L"Invalid duration.\n";
                    }
                } else if (action == L"4") {
                    chooseAnotherItem = true;
                } else if (action == L"5") {
                    chooseAnotherServer = true;
                } else if (action == L"q" || action == L"Q") {
                    return;
                } else {
                    std::wcout << L"Invalid selection.\n";
                }
            }
        }
    }
}

} // namespace opcclient
