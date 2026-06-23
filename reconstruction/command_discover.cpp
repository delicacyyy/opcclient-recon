#include "commands.h"
#include "opc_client.h"

namespace opcclient {

void Discover(const Options& options, const RuntimeSettings& settings) {
    const std::wstring& host = settings.host;
    int limit = options.GetInt(L"--limit", 100);
    OpcSession session(host, settings.config.get());
    session.ConnectServerList();
    CATID category = CATID_OPCDAServer20;
    ComPtr<IEnumGUID> enumerator;
    Check(
        session.ServerList()->EnumClassesOfCategories(1, &category, 0, nullptr, enumerator.Put()),
        L"IOPCServerList::EnumClassesOfCategories"
    );
    Check(session.Blanket(enumerator.Get()), L"CoSetProxyBlanket(server enumerator)");

    int emitted = 0;
    while (emitted < limit) {
        CLSID clsid{};
        ULONG fetched = 0;
        HRESULT hr = enumerator->Next(1, &clsid, &fetched);
        if (hr == S_FALSE || fetched == 0) {
            break;
        }
        Check(hr, L"IEnumGUID::Next");
        LPOLESTR progId = nullptr;
        LPOLESTR userType = nullptr;
        hr = session.ServerList()->GetClassDetails(clsid, &progId, &userType);
        std::wcout << L"clsid=" << GuidText(clsid);
        if (SUCCEEDED(hr)) {
            std::wcout << L" progid=" << (progId ? progId : L"")
                       << L" user_type=" << (userType ? userType : L"");
        } else {
            std::wcout << L" details_error=" << HResultText(hr);
        }
        std::wcout << L"\n";
        CoTaskMemFree(progId);
        CoTaskMemFree(userType);
        ++emitted;
    }
    std::wcout << L"servers_enumerated=" << emitted << L"\n";
}

} // namespace opcclient
