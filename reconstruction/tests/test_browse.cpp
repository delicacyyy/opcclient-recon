#include "pch.h"

#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestBrowseReturnsItems() {
    auto config = MakeLiveConfig();
    auto session = ConnectLiveSession(config.get());

    ComPtr<IOPCBrowseServerAddressSpace> browser;
    RequireSucceeded(
        QueryRemoteInterface(
            session->Server(),
            IID_IOPCBrowseServerAddressSpace,
            config.get(),
            browser.Put()
        ),
        L"QueryInterface(IOPCBrowseServerAddressSpace)"
    );

    OPCNAMESPACETYPE namespaceType{};
    RequireSucceeded(browser->QueryOrganization(&namespaceType),
                     L"IOPCBrowseServerAddressSpace::QueryOrganization");
    std::wcout << L"namespace.type="
               << (namespaceType == OPC_NS_HIERARCHIAL ? L"hierarchical" : L"flat")
               << L"\n";

    ComPtr<IEnumString> strings;
    RequireSucceeded(
        browser->BrowseOPCItemIDs(OPC_FLAT, L"", VT_EMPTY, 0, strings.Put()),
        L"IOPCBrowseServerAddressSpace::BrowseOPCItemIDs"
    );
    RequireSucceeded(session->Blanket(strings.Get()), L"CoSetProxyBlanket(item enumerator)");

    ULONG count = 0;
    bool sawWritableItem = false;
    for (;;) {
        LPOLESTR item = nullptr;
        ULONG fetched = 0;
        HRESULT hr = strings->Next(1, &item, &fetched);
        if (hr == S_FALSE || fetched == 0) {
            break;
        }
        RequireSucceeded(hr, L"IEnumString::Next");
        if (item && std::wstring(item) == kWritableItem) {
            sawWritableItem = true;
        }
        CoTaskMemFree(item);
        ++count;
    }

    std::wcout << L"browse.items=" << count
               << L" browse.saw_writable_item=" << (sawWritableItem ? L"true" : L"false")
               << L"\n";
    Require(count > 0, "browse returned no items");
}

} // namespace opcclient::tests
