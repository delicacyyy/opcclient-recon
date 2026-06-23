#include "commands.h"
#include "opc_client.h"

namespace opcclient {

void Read(const Options& options, const RuntimeSettings& settings) {
    const std::wstring& host = settings.host;
    std::wstring progId = Required(options, L"--progid", L"Kepware.KEPServerEX.V6");
    std::wstring itemId = Required(options, L"--item", L"_System._Time");
    std::wstring mode = Required(options, L"--mode", L"sync");
    OpcSession session(host, settings.config.get());
    session.Connect(progId);
    PrintItemProperties(session.Server(), itemId, settings.config.get());
    OpcGroup group(session.Server(), itemId, settings.config.get());

    if (mode == L"sync") {
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
        return;
    }

    if (mode == L"async") {
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
            std::wcout << L"async.transaction=" << transactionId
                       << L" async.cancel_id=" << cancelId
                       << L" async.immediate_error=" << HResultText(itemError) << L"\n";
            CoTaskMemFree(errors);
            if (FAILED(itemError)) {
                Check(itemError, L"Async item read");
            }
            DWORD wait = callback->Wait(15000);
            if (wait != WAIT_OBJECT_0 || !callback->ReadCompleted()) {
                throw std::runtime_error("Timed out waiting for OnReadComplete");
            }
        } catch (...) {
            callback->Release();
            throw;
        }
        callback->Release();
        return;
    }

    throw std::runtime_error("Unsupported read mode; use sync or async");
}

} // namespace opcclient
