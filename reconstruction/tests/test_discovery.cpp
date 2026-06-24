#include "pch.h"

#include "live_opc_fixture.h"

namespace opcclient::tests {

namespace {

struct DiscoveryCategory {
    CATID id;
    const wchar_t* label;
};

} // namespace

void TestDiscoveryEnumeratesOpcDaServers() {
    auto config = MakeLiveConfig();
    OpcSession session(kHost, config.get());
    session.ConnectServerList();

    const DiscoveryCategory categories[] = {
        {CATID_OPCDAServer30, L"DA 3.0"},
        {CATID_OPCDAServer20, L"DA 2.0"},
        {CATID_OPCDAServer10, L"DA 1.0"},
    };

    std::set<std::wstring> discovered;

    for (const DiscoveryCategory& category : categories) {
        ComPtr<IEnumGUID> enumerator;
        CATID categoryId = category.id;
        HRESULT hr = session.ServerList()->EnumClassesOfCategories(
            1,
            &categoryId,
            0,
            nullptr,
            enumerator.Put()
        );
        if (hr == S_FALSE || !enumerator) {
            std::wcout << L"discovery.category=" << category.label
                       << L" result=no_classes\n";
            continue;
        }
        RequireSucceeded(hr, L"IOPCServerList::EnumClassesOfCategories");
        RequireSucceeded(session.Blanket(enumerator.Get()), L"CoSetProxyBlanket(server enumerator)");

        for (;;) {
            CLSID clsid{};
            ULONG fetched = 0;
            hr = enumerator->Next(1, &clsid, &fetched);
            if (hr == S_FALSE || fetched == 0) {
                break;
            }
            RequireSucceeded(hr, L"IEnumGUID::Next");

            std::wstring clsidText = GuidText(clsid);
            discovered.insert(clsidText);

            LPOLESTR progId = nullptr;
            LPOLESTR userType = nullptr;
            HRESULT detailsHr = session.ServerList()->GetClassDetails(clsid, &progId, &userType);
            std::wcout << L"discovery.category=" << category.label
                       << L" clsid=" << clsidText;
            if (SUCCEEDED(detailsHr)) {
                std::wcout << L" progid=" << (progId ? progId : L"")
                           << L" user_type=" << (userType ? userType : L"");
            } else {
                std::wcout << L" details_error=" << HResultText(detailsHr);
            }
            std::wcout << L"\n";

            CoTaskMemFree(progId);
            CoTaskMemFree(userType);
        }
    }

    std::wcout << L"discovery.unique_servers=" << discovered.size() << L"\n";
    Require(!discovered.empty(), "OPCEnum returned no OPC DA servers");
}

} // namespace opcclient::tests
