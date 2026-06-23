#include "commands.h"
#include "opc_client.h"

namespace opcclient {

void Subscribe(const Options& options, const RuntimeSettings& settings) {
    const std::wstring& host = settings.host;
    std::wstring progId = Required(options, L"--progid", L"Kepware.KEPServerEX.V6");
    std::wstring itemId = Required(options, L"--item", L"_System._Time");
    int seconds = options.GetInt(L"--seconds", 10);
    if (seconds < 1 || seconds > 3600) {
        throw std::runtime_error("--seconds must be between 1 and 3600");
    }

    OpcSession session(host, settings.config.get());
    session.Connect(progId);
    OpcGroup group(session.Server(), itemId, settings.config.get(), 1000);
    auto* callback = new OpcCallback();
    try {
        Check(group.Advise(callback), L"Advise(IOPCDataCallback)");
        ComPtr<IOPCAsyncIO2> async;
        Check(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");
        Check(async->SetEnable(TRUE), L"IOPCAsyncIO2::SetEnable");
        std::wcout << L"subscription.seconds=" << seconds
                   << L" subscription.update_rate_ms=" << group.RevisedUpdateRate() << L"\n";
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        std::wcout << L"subscription.data_change_items=" << callback->DataChanges() << L"\n";
    } catch (...) {
        callback->Release();
        throw;
    }
    callback->Release();
}

} // namespace opcclient
