#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestDiscoveryEnumeratesKepware() {
    auto config = MakeLiveConfig();
    OpcSession session(kHost, config.get());
    session.ConnectServerList();

    CATID category = CATID_OPCDAServer20;
    ComPtr<IEnumGUID> enumerator;
    RequireSucceeded(
        session.ServerList()->EnumClassesOfCategories(1, &category, 0, nullptr, enumerator.Put()),
        L"IOPCServerList::EnumClassesOfCategories"
    );
    RequireSucceeded(session.Blanket(enumerator.Get()), L"CoSetProxyBlanket(server enumerator)");

    bool found = false;
    for (;;) {
        CLSID clsid{};
        ULONG fetched = 0;
        HRESULT hr = enumerator->Next(1, &clsid, &fetched);
        if (hr == S_FALSE || fetched == 0) {
            break;
        }
        RequireSucceeded(hr, L"IEnumGUID::Next");

        LPOLESTR progId = nullptr;
        LPOLESTR userType = nullptr;
        hr = session.ServerList()->GetClassDetails(clsid, &progId, &userType);
        if (SUCCEEDED(hr) && progId && std::wstring(progId) == kProgId) {
            found = true;
        }
        CoTaskMemFree(progId);
        CoTaskMemFree(userType);
    }

    Require(found, "Kepware.KEPServerEX.V6 was not discovered");
}

} // namespace opcclient::tests
