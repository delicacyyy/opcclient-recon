#include "live_opc_fixture.h"

namespace opcclient::tests {

const wchar_t kHost[] = L"10.66.86.123";
const wchar_t kDomain[] = L"JWLG240393";
const wchar_t kUsername[] = L"opcuser1";
const wchar_t kPasswordPlaceholder[] = L"<FILL_PASSWORD_HERE>";

// Live tests intentionally keep this value in source per the current test plan.
// Replace the placeholder locally before running the test executable.
const wchar_t kPassword[] = L"<FILL_PASSWORD_HERE>";

const wchar_t kProgId[] = L"Kepware.KEPServerEX.V6";
const wchar_t kWritableItem[] = L"通道 1.设备 1.标记 1";
const USHORT kAlternateUi2Value = 5;

namespace {

void FreeErrors(HRESULT* errors) {
    CoTaskMemFree(errors);
}

void FreeReadResults(OPCITEMSTATE* values, HRESULT* errors, DWORD count) {
    if (values) {
        for (DWORD index = 0; index < count; ++index) {
            VariantClear(&values[index].vDataValue);
        }
        CoTaskMemFree(values);
    }
    FreeErrors(errors);
}

VARIANT MakeUi2Variant(USHORT value) {
    VARIANT variant{};
    VariantInit(&variant);
    variant.vt = VT_UI2;
    variant.uiVal = value;
    return variant;
}

} // namespace

bool PasswordPlaceholderStillPresent() {
    return std::wstring(kPassword) == kPasswordPlaceholder;
}

std::unique_ptr<ConnectionConfig> MakeLiveConfig() {
    return ConnectionConfig::FromValues(kHost, kDomain, kUsername, kPassword);
}

std::unique_ptr<OpcSession> ConnectLiveSession(const ConnectionConfig* config) {
    auto session = std::make_unique<OpcSession>(kHost, config);
    session->Connect(kProgId);
    return session;
}

Ui2ReadResult ReadUi2(IOPCServer* server, const ConnectionConfig* config) {
    OpcGroup group(server, kWritableItem, config);

    ComPtr<IOPCSyncIO> sync;
    RequireSucceeded(group.GetSync(sync), L"QueryInterface(IOPCSyncIO)");

    OPCHANDLE handle = group.ServerItemHandle();
    OPCITEMSTATE* values = nullptr;
    HRESULT* errors = nullptr;
    HRESULT hr = sync->Read(OPC_DS_DEVICE, 1, &handle, &values, &errors);
    if (FAILED(hr)) {
        FreeReadResults(values, errors, values ? 1 : 0);
        RequireSucceeded(hr, L"IOPCSyncIO::Read");
    }
    RequireSucceeded(hr, L"IOPCSyncIO::Read");

    HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
    VARTYPE type = values ? values[0].vDataValue.vt : VT_EMPTY;
    Ui2ReadResult result{};
    if (values && type == VT_UI2) {
        result.value = values[0].vDataValue.uiVal;
        result.quality = values[0].wQuality;
        result.timestamp = values[0].ftTimeStamp;
    }
    FreeReadResults(values, errors, 1);

    RequireSucceeded(itemError, L"sync read item result");
    RequireEqual(type, VT_UI2, "expected VT_UI2 read result");
    Require((result.quality & OPC_QUALITY_MASK) != OPC_QUALITY_BAD, "read quality is bad");
    return result;
}

void WriteUi2(IOPCServer* server, const ConnectionConfig* config, USHORT value) {
    OpcGroup group(server, kWritableItem, config);
    Require((group.AccessRights() & OPC_WRITEABLE) != 0, "item is not writable");

    ComPtr<IOPCSyncIO> sync;
    RequireSucceeded(group.GetSync(sync), L"QueryInterface(IOPCSyncIO)");

    VARIANT variant = MakeUi2Variant(value);
    OPCHANDLE handle = group.ServerItemHandle();
    HRESULT* errors = nullptr;
    HRESULT hr = sync->Write(1, &handle, &variant, &errors);
    HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
    FreeErrors(errors);
    VariantClear(&variant);

    RequireSucceeded(hr, L"IOPCSyncIO::Write");
    RequireSucceeded(itemError, L"sync write item result");
}

void WriteUi2Async(IOPCServer* server, const ConnectionConfig* config, USHORT value) {
    OpcGroup group(server, kWritableItem, config);
    Require((group.AccessRights() & OPC_WRITEABLE) != 0, "item is not writable");

    auto* callback = new OpcCallback();
    try {
        RequireSucceeded(group.Advise(callback), L"Advise(IOPCDataCallback)");

        ComPtr<IOPCAsyncIO2> async;
        RequireSucceeded(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");

        callback->Reset();
        VARIANT variant = MakeUi2Variant(value);
        OPCHANDLE handle = group.ServerItemHandle();
        DWORD cancelId = 0;
        HRESULT* errors = nullptr;
        constexpr DWORD transactionId = 2001;
        HRESULT hr = async->Write(1, &handle, variant.vt == VT_EMPTY ? nullptr : &variant, transactionId, &cancelId, &errors);
        HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
        FreeErrors(errors);
        VariantClear(&variant);

        RequireSucceeded(hr, L"IOPCAsyncIO2::Write");
        RequireSucceeded(itemError, L"async write item result");

        DWORD wait = callback->Wait(15000);
        Require(wait == WAIT_OBJECT_0, "timed out waiting for OnWriteComplete");
        Require(callback->WriteCompleted(), "OnWriteComplete was not observed");
    } catch (...) {
        callback->Release();
        throw;
    }
    callback->Release();
}

USHORT ChooseTemporaryUi2Value(USHORT original) {
    return original == kAlternateUi2Value
        ? static_cast<USHORT>(kAlternateUi2Value + 1)
        : kAlternateUi2Value;
}

} // namespace opcclient::tests
