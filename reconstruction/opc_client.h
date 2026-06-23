#pragma once

#include "config.h"

namespace opcclient {

HRESULT ApplyProxyBlanket(IUnknown* proxy, const ConnectionConfig* config);

template <typename T>
HRESULT QueryRemoteInterface(
    IUnknown* source,
    REFIID iid,
    const ConnectionConfig* config,
    T** output
) {
    if (!source || !output) {
        return E_POINTER;
    }
    *output = nullptr;
    HRESULT hr = source->QueryInterface(iid, reinterpret_cast<void**>(output));
    if (FAILED(hr)) {
        return hr;
    }
    hr = ApplyProxyBlanket(*output, config);
    if (FAILED(hr)) {
        (*output)->Release();
        *output = nullptr;
    }
    return hr;
}

class ComRuntime {
public:
    ComRuntime();
    ComRuntime(const ComRuntime&) = delete;
    ComRuntime& operator=(const ComRuntime&) = delete;
    ~ComRuntime();

private:
    bool initialized_ = false;
};

class OpcCallback final : public IOPCDataCallback, public IOPCShutdown {
public:
    OpcCallback();
    ~OpcCallback();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE OnDataChange(
        DWORD transactionId,
        OPCHANDLE group,
        HRESULT masterQuality,
        HRESULT masterError,
        DWORD count,
        OPCHANDLE* clientItems,
        VARIANT* values,
        WORD* qualities,
        FILETIME* timestamps,
        HRESULT* errors
    ) override;

    HRESULT STDMETHODCALLTYPE OnReadComplete(
        DWORD transactionId,
        OPCHANDLE group,
        HRESULT masterQuality,
        HRESULT masterError,
        DWORD count,
        OPCHANDLE* clientItems,
        VARIANT* values,
        WORD* qualities,
        FILETIME* timestamps,
        HRESULT* errors
    ) override;

    HRESULT STDMETHODCALLTYPE OnWriteComplete(
        DWORD transactionId,
        OPCHANDLE group,
        HRESULT masterError,
        DWORD count,
        OPCHANDLE* clientHandles,
        HRESULT* errors
    ) override;

    HRESULT STDMETHODCALLTYPE OnCancelComplete(DWORD transactionId, OPCHANDLE group) override;
    HRESULT STDMETHODCALLTYPE ShutdownRequest(LPCWSTR reason) override;

    void Reset();
    DWORD Wait(DWORD milliseconds) const;
    bool ReadCompleted() const;
    bool WriteCompleted() const;
    ULONG DataChanges() const;

private:
    void PrintValues(
        const wchar_t* callback,
        DWORD transactionId,
        OPCHANDLE group,
        HRESULT masterQuality,
        HRESULT masterError,
        DWORD count,
        OPCHANDLE* clientItems,
        VARIANT* values,
        WORD* qualities,
        FILETIME* timestamps,
        HRESULT* errors
    );

    std::atomic<ULONG> references_{1};
    HANDLE event_ = nullptr;
    std::atomic<bool> readCompleted_{false};
    std::atomic<bool> writeCompleted_{false};
    std::atomic<bool> shutdownRequested_{false};
    std::atomic<ULONG> dataChanges_{0};
    std::mutex outputMutex_;
};

class AdviseConnection {
public:
    AdviseConnection() = default;
    AdviseConnection(const AdviseConnection&) = delete;
    AdviseConnection& operator=(const AdviseConnection&) = delete;
    ~AdviseConnection();

    HRESULT Connect(
        IUnknown* source,
        REFIID callbackInterface,
        IUnknown* sink,
        const ConnectionConfig* config
    );
    void Disconnect();

private:
    ComPtr<IConnectionPoint> point_;
    DWORD cookie_ = 0;
};

class OpcSession {
public:
    explicit OpcSession(std::wstring host, const ConnectionConfig* config = nullptr);

    void ConnectServerList();
    void Connect(const std::wstring& progId);
    void Connect(REFCLSID serverClass, const std::wstring& progId);

    IOPCServer* Server() const;
    IOPCServerList* ServerList() const;
    const CLSID& ServerClsid() const;
    const ConnectionConfig* Config() const;
    HRESULT Blanket(IUnknown* proxy) const;

private:
    std::wstring host_;
    std::wstring progId_;
    CLSID serverClsid_{};
    const ConnectionConfig* config_ = nullptr;
    ComPtr<IOPCServerList> serverList_;
    ComPtr<IOPCServer> server_;
};

class OpcGroup {
public:
    OpcGroup(
        IOPCServer* server,
        const std::wstring& itemId,
        const ConnectionConfig* config = nullptr,
        DWORD requestedUpdateRate = 1000
    );
    OpcGroup(const OpcGroup&) = delete;
    OpcGroup& operator=(const OpcGroup&) = delete;
    ~OpcGroup();

    OPCHANDLE ServerItemHandle() const;
    DWORD AccessRights() const;
    VARTYPE CanonicalType() const;
    DWORD RevisedUpdateRate() const;
    IUnknown* GroupUnknown() const;
    HRESULT Advise(OpcCallback* callback);
    HRESULT GetSync(ComPtr<IOPCSyncIO>& output);
    HRESULT GetAsync(ComPtr<IOPCAsyncIO2>& output);

private:
    static constexpr OPCHANDLE kClientGroupHandle = 1;
    static constexpr OPCHANDLE kClientItemHandle = 1;

    std::wstring itemId_;
    const ConnectionConfig* config_ = nullptr;
    ComPtr<IOPCServer> server_;
    ComPtr<IUnknown> groupUnknown_;
    ComPtr<IOPCItemMgt> itemMgt_;
    AdviseConnection callbackConnection_;
    OPCHANDLE serverGroupHandle_ = 0;
    OPCHANDLE serverItemHandle_ = 0;
    DWORD revisedUpdateRate_ = 0;
    DWORD accessRights_ = 0;
    VARTYPE canonicalType_ = VT_EMPTY;
    bool itemAdded_ = false;
};

void PrintSyncRead(const OPCITEMSTATE& state, HRESULT itemError);
void PrintItemProperties(
    IOPCServer* server,
    const std::wstring& itemId,
    const ConnectionConfig* config
);

} // namespace opcclient
