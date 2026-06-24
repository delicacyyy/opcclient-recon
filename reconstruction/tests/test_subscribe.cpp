#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestSubscribeSetupSucceeds() {
    auto config = MakeLiveConfig();
    auto session = ConnectLiveSession(config.get());
    OpcGroup group(session->Server(), kWritableItem, config.get(), 1000);

    auto* callback = new OpcCallback();
    try {
        RequireSucceeded(group.Advise(callback), L"Advise(IOPCDataCallback)");

        ComPtr<IOPCAsyncIO2> async;
        RequireSucceeded(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");
        RequireSucceeded(async->SetEnable(TRUE), L"IOPCAsyncIO2::SetEnable");

        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::wcout << L"subscription.data_change_items=" << callback->DataChanges()
                   << L" subscription.update_rate_ms=" << group.RevisedUpdateRate()
                   << L"\n";
    } catch (...) {
        callback->Release();
        throw;
    }
    callback->Release();
}

} // namespace opcclient::tests
