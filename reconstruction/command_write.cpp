#include "commands.h"
#include "opc_client.h"

namespace opcclient {
namespace {

VARIANT ParseVariant(const std::wstring& type, const std::wstring& text) {
    VARIANT value{};
    VariantInit(&value);
    if (type == L"bool") {
        value.vt = VT_BOOL;
        value.boolVal = (text == L"1" || text == L"true" || text == L"TRUE") ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (type == L"i2") {
        value.vt = VT_I2;
        value.iVal = static_cast<SHORT>(std::stoi(text));
    } else if (type == L"i4") {
        value.vt = VT_I4;
        value.lVal = std::stol(text);
    } else if (type == L"ui2") {
        value.vt = VT_UI2;
        value.uiVal = static_cast<USHORT>(std::stoul(text));
    } else if (type == L"ui4") {
        value.vt = VT_UI4;
        value.ulVal = std::stoul(text);
    } else if (type == L"r4") {
        value.vt = VT_R4;
        value.fltVal = std::stof(text);
    } else if (type == L"r8") {
        value.vt = VT_R8;
        value.dblVal = std::stod(text);
    } else if (type == L"string") {
        value.vt = VT_BSTR;
        value.bstrVal = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
        if (!value.bstrVal) {
            throw std::bad_alloc();
        }
    } else {
        throw std::runtime_error("Unsupported --type");
    }
    return value;
}

} // namespace

void Write(const Options& options, const RuntimeSettings& settings) {
    if (!options.Has(L"--allow-write")) {
        throw std::runtime_error("Write rejected: --allow-write is required");
    }
    const std::wstring& host = settings.host;
    std::wstring progId = Required(options, L"--progid", L"Kepware.KEPServerEX.V6");
    std::wstring itemId = Required(options, L"--item");
    std::wstring mode = Required(options, L"--mode", L"sync");
    std::wstring type = Required(options, L"--type");
    std::wstring text = Required(options, L"--value");

    OpcSession session(host, settings.config.get());
    session.Connect(progId);
    OpcGroup group(session.Server(), itemId, settings.config.get());
    if ((group.AccessRights() & OPC_WRITEABLE) == 0) {
        throw std::runtime_error("Server reports that the item is not writable");
    }

    VARIANT value = ParseVariant(type, text);
    try {
        OPCHANDLE handle = group.ServerItemHandle();
        if (mode == L"sync") {
            ComPtr<IOPCSyncIO> sync;
            Check(group.GetSync(sync), L"QueryInterface(IOPCSyncIO)");
            HRESULT* errors = nullptr;
            Check(sync->Write(1, &handle, &value, &errors), L"IOPCSyncIO::Write");
            HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
            CoTaskMemFree(errors);
            Check(itemError, L"Synchronous item write");
            std::wcout << L"write.mode=sync write.result=success\n";
        } else if (mode == L"async") {
            auto* callback = new OpcCallback();
            try {
                Check(group.Advise(callback), L"Advise(IOPCDataCallback)");
                ComPtr<IOPCAsyncIO2> async;
                Check(group.GetAsync(async), L"QueryInterface(IOPCAsyncIO2)");
                callback->Reset();
                DWORD cancelId = 0;
                HRESULT* errors = nullptr;
                constexpr DWORD transactionId = 2001;
                Check(async->Write(1, &handle, &value, transactionId, &cancelId, &errors), L"IOPCAsyncIO2::Write");
                HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
                CoTaskMemFree(errors);
                Check(itemError, L"Asynchronous item write");
                DWORD wait = callback->Wait(15000);
                if (wait != WAIT_OBJECT_0 || !callback->WriteCompleted()) {
                    throw std::runtime_error("Timed out waiting for OnWriteComplete");
                }
            } catch (...) {
                callback->Release();
                throw;
            }
            callback->Release();
        } else {
            throw std::runtime_error("Unsupported write mode; use sync or async");
        }
    } catch (...) {
        VariantClear(&value);
        throw;
    }
    VariantClear(&value);
}

} // namespace opcclient
