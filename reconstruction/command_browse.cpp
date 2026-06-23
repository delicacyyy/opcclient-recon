#include "commands.h"
#include "opc_client.h"

namespace opcclient {

void Browse(const Options& options, const RuntimeSettings& settings) {
    const std::wstring& host = settings.host;
    std::wstring progId = Required(options, L"--progid", L"Kepware.KEPServerEX.V6");
    int limit = options.GetInt(L"--limit", 100);
    OpcSession session(host, settings.config.get());
    session.Connect(progId);

    ComPtr<IOPCBrowseServerAddressSpace> browser;
    Check(
        QueryRemoteInterface(
            session.Server(),
            IID_IOPCBrowseServerAddressSpace,
            settings.config.get(),
            browser.Put()
        ),
        L"QueryInterface(IOPCBrowseServerAddressSpace)"
    );
    OPCNAMESPACETYPE namespaceType{};
    Check(browser->QueryOrganization(&namespaceType), L"IOPCBrowseServerAddressSpace::QueryOrganization");
    std::wcout << L"namespace.type="
               << (namespaceType == OPC_NS_HIERARCHIAL ? L"hierarchical" : L"flat") << L"\n";

    ComPtr<IEnumString> strings;
    Check(
        browser->BrowseOPCItemIDs(OPC_FLAT, L"", VT_EMPTY, 0, strings.Put()),
        L"IOPCBrowseServerAddressSpace::BrowseOPCItemIDs"
    );
    Check(session.Blanket(strings.Get()), L"CoSetProxyBlanket(item enumerator)");
    int emitted = 0;
    while (emitted < limit) {
        LPOLESTR item = nullptr;
        ULONG fetched = 0;
        HRESULT hr = strings->Next(1, &item, &fetched);
        if (hr == S_FALSE || fetched == 0) {
            break;
        }
        Check(hr, L"IEnumString::Next");
        std::wcout << L"item=" << (item ? item : L"") << L"\n";
        CoTaskMemFree(item);
        ++emitted;
    }
    std::wcout << L"items_emitted=" << emitted << L" limit=" << limit << L"\n";
}

} // namespace opcclient
