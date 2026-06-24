#include "pch.h"

#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestReadAsyncCompletesCallback() {
    auto config = MakeLiveConfig();
    auto session = ConnectLiveSession(config.get());
    OpcGroup group(session->Server(), kWritableItem, config.get());

    auto* callback = new OpcCallback();
    try {
        RequireSucceeded(group.Advise(callback), L"Advise(IOPCDataCallback)");

        ComPtr<IOPCAsyncIO2> async;
        RequireSucceeded(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");
        RequireSucceeded(async->SetEnable(TRUE), L"IOPCAsyncIO2::SetEnable");

        callback->Reset();
        OPCHANDLE handle = group.ServerItemHandle();
        DWORD cancelId = 0;
        HRESULT* errors = nullptr;
        constexpr DWORD transactionId = 1001;
        HRESULT hr = async->Read(1, &handle, transactionId, &cancelId, &errors);
        HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
        CoTaskMemFree(errors);

        RequireSucceeded(hr, L"IOPCAsyncIO2::Read");
        RequireSucceeded(itemError, L"async read item result");

        DWORD wait = callback->Wait(15000);
        Require(wait == WAIT_OBJECT_0, "timed out waiting for OnReadComplete");
        Require(callback->ReadCompleted(), "OnReadComplete was not observed");
    } catch (...) {
        callback->Release();
        throw;
    }
    callback->Release();
}

} // namespace opcclient::tests
