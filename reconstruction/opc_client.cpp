#include "opc_client.h"

namespace opcclient {
namespace {

COAUTHINFO MakeAuthInfo(COAUTHIDENTITY* identity) {
    COAUTHINFO auth{};
    auth.dwAuthnSvc = RPC_C_AUTHN_WINNT;
    auth.dwAuthzSvc = RPC_C_AUTHZ_NONE;
    auth.pwszServerPrincName = nullptr;
    auth.dwAuthnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY;
    auth.dwImpersonationLevel = RPC_C_IMP_LEVEL_IMPERSONATE;
    auth.pAuthIdentityData = identity;
    auth.dwCapabilities = EOAC_NONE;
    return auth;
}

template <typename T>
HRESULT RemoteCreate(
    const std::wstring& host,
    const ConnectionConfig* config,
    REFCLSID clsid,
    REFIID iid,
    T** output
) {
    if (!output) {
        return E_POINTER;
    }
    *output = nullptr;
    COAUTHIDENTITY identity{};
    COAUTHINFO auth{};
    COSERVERINFO serverInfo{};
    serverInfo.pwszName = const_cast<LPWSTR>(host.c_str());
    if (config) {
        identity = config->Identity();
        auth = MakeAuthInfo(&identity);
        serverInfo.pAuthInfo = &auth;
    }
    MULTI_QI query{};
    query.pIID = &iid;
    HRESULT hr = CoCreateInstanceEx(
        clsid,
        nullptr,
        CLSCTX_REMOTE_SERVER | CLSCTX_LOCAL_SERVER,
        &serverInfo,
        1,
        &query
    );
    if (FAILED(hr)) {
        return hr;
    }
    if (FAILED(query.hr)) {
        return query.hr;
    }
    *output = static_cast<T*>(query.pItf);
    HRESULT blanket = ApplyProxyBlanket(*output, config);
    if (FAILED(blanket)) {
        (*output)->Release();
        *output = nullptr;
        return blanket;
    }
    return S_OK;
}

} // namespace

HRESULT ApplyProxyBlanket(IUnknown* proxy, const ConnectionConfig* config) {
    if (!proxy || !config) {
        return S_OK;
    }
    COAUTHIDENTITY identity = config->Identity();
    return CoSetProxyBlanket(
        proxy,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        &identity,
        EOAC_NONE
    );
}

ComRuntime::ComRuntime() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        Check(hr, L"CoInitializeEx");
    }
    initialized_ = true;

    hr = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr
    );
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        Check(hr, L"CoInitializeSecurity");
    }
}

ComRuntime::~ComRuntime() {
    if (initialized_) {
        CoUninitialize();
    }
}

OpcCallback::OpcCallback() {
    event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event_) {
        throw std::runtime_error("CreateEventW failed");
    }
}

OpcCallback::~OpcCallback() {
    CloseHandle(event_);
}

HRESULT STDMETHODCALLTYPE OpcCallback::QueryInterface(REFIID iid, void** object) {
    if (!object) {
        return E_POINTER;
    }
    *object = nullptr;
    if (iid == IID_IUnknown || iid == IID_IOPCDataCallback) {
        *object = static_cast<IOPCDataCallback*>(this);
    } else if (iid == IID_IOPCShutdown) {
        *object = static_cast<IOPCShutdown*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE OpcCallback::AddRef() {
    return ++references_;
}

ULONG STDMETHODCALLTYPE OpcCallback::Release() {
    ULONG remaining = --references_;
    if (!remaining) {
        delete this;
    }
    return remaining;
}

HRESULT STDMETHODCALLTYPE OpcCallback::OnDataChange(
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
) {
    PrintValues(L"OnDataChange", transactionId, group, masterQuality, masterError, count,
                clientItems, values, qualities, timestamps, errors);
    dataChanges_ += count;
    SetEvent(event_);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE OpcCallback::OnReadComplete(
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
) {
    PrintValues(L"OnReadComplete", transactionId, group, masterQuality, masterError, count,
                clientItems, values, qualities, timestamps, errors);
    readCompleted_ = true;
    SetEvent(event_);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE OpcCallback::OnWriteComplete(
    DWORD transactionId,
    OPCHANDLE group,
    HRESULT masterError,
    DWORD count,
    OPCHANDLE* clientHandles,
    HRESULT* errors
) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    std::wcout << L"callback=OnWriteComplete transaction=" << transactionId
               << L" group=" << group
               << L" master_error=" << HResultText(masterError) << L"\n";
    for (DWORD i = 0; i < count; ++i) {
        std::wcout << L"  item_client_handle=" << clientHandles[i]
                   << L" error=" << HResultText(errors[i]) << L"\n";
    }
    writeCompleted_ = true;
    SetEvent(event_);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE OpcCallback::OnCancelComplete(DWORD transactionId, OPCHANDLE group) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    std::wcout << L"callback=OnCancelComplete transaction=" << transactionId
               << L" group=" << group << L"\n";
    SetEvent(event_);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE OpcCallback::ShutdownRequest(LPCWSTR reason) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    std::wcout << L"callback=ShutdownRequest reason=" << (reason ? reason : L"") << L"\n";
    shutdownRequested_ = true;
    SetEvent(event_);
    return S_OK;
}

void OpcCallback::Reset() {
    ResetEvent(event_);
    readCompleted_ = false;
    writeCompleted_ = false;
}

DWORD OpcCallback::Wait(DWORD milliseconds) const {
    return WaitForSingleObject(event_, milliseconds);
}

bool OpcCallback::ReadCompleted() const {
    return readCompleted_;
}

bool OpcCallback::WriteCompleted() const {
    return writeCompleted_;
}

ULONG OpcCallback::DataChanges() const {
    return dataChanges_;
}

void OpcCallback::PrintValues(
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
) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    std::wcout << L"callback=" << callback
               << L" transaction=" << transactionId
               << L" group=" << group
               << L" master_quality=" << HResultText(masterQuality)
               << L" master_error=" << HResultText(masterError) << L"\n";
    for (DWORD i = 0; i < count; ++i) {
        std::wcout << L"  item_client_handle=" << clientItems[i]
                   << L" value=" << VariantText(values[i])
                   << L" vartype=" << VariantTypeText(values[i].vt)
                   << L" quality=" << QualityText(qualities[i])
                   << L" timestamp=" << FileTimeText(timestamps[i])
                   << L" error=" << HResultText(errors[i]) << L"\n";
    }
}

AdviseConnection::~AdviseConnection() {
    Disconnect();
}

HRESULT AdviseConnection::Connect(
    IUnknown* source,
    REFIID callbackInterface,
    IUnknown* sink,
    const ConnectionConfig* config
) {
    Disconnect();
    if (!source || !sink) {
        return E_POINTER;
    }
    ComPtr<IConnectionPointContainer> container;
    HRESULT hr = QueryRemoteInterface(
        source,
        IID_IConnectionPointContainer,
        config,
        container.Put()
    );
    if (FAILED(hr)) {
        return hr;
    }
    hr = container->FindConnectionPoint(callbackInterface, point_.Put());
    if (FAILED(hr)) {
        return hr;
    }
    hr = ApplyProxyBlanket(point_.Get(), config);
    if (FAILED(hr)) {
        point_.Reset();
        return hr;
    }
    hr = point_->Advise(sink, &cookie_);
    if (FAILED(hr)) {
        point_.Reset();
    }
    return hr;
}

void AdviseConnection::Disconnect() {
    if (point_ && cookie_) {
        point_->Unadvise(cookie_);
    }
    cookie_ = 0;
    point_.Reset();
}

OpcSession::OpcSession(std::wstring host, const ConnectionConfig* config)
    : host_(std::move(host)), config_(config) {}

void OpcSession::ConnectServerList() {
    if (serverList_) {
        return;
    }
    Check(
        RemoteCreate(host_, config_, CLSID_OpcServerList, IID_IOPCServerList, serverList_.Put()),
        L"Remote activation of OPCEnum"
    );
}

void OpcSession::Connect(const std::wstring& progId) {
    ConnectServerList();
    CLSID serverClass{};
    Check(serverList_->CLSIDFromProgID(progId.c_str(), &serverClass), L"IOPCServerList::CLSIDFromProgID");
    Connect(serverClass, progId);
}

void OpcSession::Connect(REFCLSID serverClass, const std::wstring& progId) {
    Check(
        RemoteCreate(host_, config_, serverClass, IID_IOPCServer, server_.Put()),
        L"Remote OPC server activation"
    );
    serverClsid_ = serverClass;
    progId_ = progId;
}

IOPCServer* OpcSession::Server() const {
    return server_.Get();
}

IOPCServerList* OpcSession::ServerList() const {
    return serverList_.Get();
}

const CLSID& OpcSession::ServerClsid() const {
    return serverClsid_;
}

const ConnectionConfig* OpcSession::Config() const {
    return config_;
}

HRESULT OpcSession::Blanket(IUnknown* proxy) const {
    return ApplyProxyBlanket(proxy, config_);
}

OpcGroup::OpcGroup(
    IOPCServer* server,
    const std::wstring& itemId,
    const ConnectionConfig* config,
    DWORD requestedUpdateRate
) : itemId_(itemId), config_(config) {
    server_.CopyFrom(server);
    LPUNKNOWN groupUnknown = nullptr;
    DWORD revised = 0;
    Check(
        server_->AddGroup(
            L"OPCClientReconGroup",
            TRUE,
            requestedUpdateRate,
            kClientGroupHandle,
            nullptr,
            nullptr,
            LOCALE_SYSTEM_DEFAULT,
            &serverGroupHandle_,
            &revised,
            IID_IUnknown,
            &groupUnknown
        ),
        L"IOPCServer::AddGroup"
    );
    groupUnknown_.Attach(groupUnknown);
    Check(ApplyProxyBlanket(groupUnknown_.Get(), config_), L"CoSetProxyBlanket(group)");
    revisedUpdateRate_ = revised;
    Check(QueryRemoteInterface(groupUnknown_.Get(), IID_IOPCItemMgt, config_, itemMgt_.Put()),
          L"QueryInterface(IOPCItemMgt)");

    OPCITEMDEF definition{};
    definition.szAccessPath = const_cast<LPWSTR>(L"");
    definition.szItemID = const_cast<LPWSTR>(itemId_.c_str());
    definition.bActive = TRUE;
    definition.hClient = kClientItemHandle;
    definition.vtRequestedDataType = VT_EMPTY;

    OPCITEMRESULT* results = nullptr;
    HRESULT* errors = nullptr;
    HRESULT hr = itemMgt_->AddItems(1, &definition, &results, &errors);
    Check(hr, L"IOPCItemMgt::AddItems");
    HRESULT itemError = errors ? errors[0] : E_UNEXPECTED;
    if (FAILED(itemError)) {
        if (results) {
            if (results[0].pBlob) {
                CoTaskMemFree(results[0].pBlob);
            }
            CoTaskMemFree(results);
        }
        if (errors) {
            CoTaskMemFree(errors);
        }
        Check(itemError, L"AddItems item result");
    }

    serverItemHandle_ = results[0].hServer;
    canonicalType_ = results[0].vtCanonicalDataType;
    accessRights_ = results[0].dwAccessRights;
    if (results[0].pBlob) {
        CoTaskMemFree(results[0].pBlob);
    }
    CoTaskMemFree(results);
    CoTaskMemFree(errors);
    itemAdded_ = true;

    std::wcout << L"group.server_handle=" << serverGroupHandle_
               << L" group.revised_update_rate_ms=" << revisedUpdateRate_ << L"\n"
               << L"item.id=" << itemId_
               << L" item.server_handle=" << serverItemHandle_
               << L" item.canonical_type=" << VariantTypeText(canonicalType_)
               << L" item.access_rights=0x" << std::hex << std::uppercase << accessRights_
               << std::dec << L"\n";
}

OpcGroup::~OpcGroup() {
    callbackConnection_.Disconnect();
    if (itemAdded_ && itemMgt_) {
        HRESULT* errors = nullptr;
        itemMgt_->RemoveItems(1, &serverItemHandle_, &errors);
        if (errors) {
            CoTaskMemFree(errors);
        }
    }
    itemMgt_.Reset();
    groupUnknown_.Reset();
    if (server_ && serverGroupHandle_) {
        server_->RemoveGroup(serverGroupHandle_, TRUE);
    }
}

OPCHANDLE OpcGroup::ServerItemHandle() const {
    return serverItemHandle_;
}

DWORD OpcGroup::AccessRights() const {
    return accessRights_;
}

VARTYPE OpcGroup::CanonicalType() const {
    return canonicalType_;
}

DWORD OpcGroup::RevisedUpdateRate() const {
    return revisedUpdateRate_;
}

IUnknown* OpcGroup::GroupUnknown() const {
    return groupUnknown_.Get();
}

HRESULT OpcGroup::Advise(OpcCallback* callback) {
    return callbackConnection_.Connect(
        groupUnknown_.Get(),
        IID_IOPCDataCallback,
        static_cast<IOPCDataCallback*>(callback),
        config_
    );
}

HRESULT OpcGroup::GetSync(ComPtr<IOPCSyncIO>& output) {
    return QueryRemoteInterface(groupUnknown_.Get(), IID_IOPCSyncIO, config_, output.Put());
}

HRESULT OpcGroup::GetAsync(ComPtr<IOPCAsyncIO2>& output) {
    return QueryRemoteInterface(groupUnknown_.Get(), IID_IOPCAsyncIO2, config_, output.Put());
}

void PrintSyncRead(const OPCITEMSTATE& state, HRESULT itemError) {
    std::wcout << L"read.error=" << HResultText(itemError) << L"\n"
               << L"read.client_handle=" << state.hClient << L"\n"
               << L"read.value=" << VariantText(state.vDataValue) << L"\n"
               << L"read.vartype=" << VariantTypeText(state.vDataValue.vt) << L"\n"
               << L"read.quality=" << QualityText(state.wQuality) << L"\n"
               << L"read.timestamp=" << FileTimeText(state.ftTimeStamp) << L"\n";
}

void PrintItemProperties(
    IOPCServer* server,
    const std::wstring& itemId,
    const ConnectionConfig* config
) {
    ComPtr<IOPCItemProperties> properties;
    HRESULT hr = QueryRemoteInterface(
        server,
        IID_IOPCItemProperties,
        config,
        properties.Put()
    );
    if (hr == E_NOINTERFACE) {
        std::wcout << L"properties.supported=false\n";
        return;
    }
    Check(hr, L"QueryInterface(IOPCItemProperties)");

    DWORD count = 0;
    DWORD* propertyIds = nullptr;
    LPWSTR* descriptions = nullptr;
    VARTYPE* dataTypes = nullptr;
    hr = properties->QueryAvailableProperties(
        const_cast<LPWSTR>(itemId.c_str()),
        &count,
        &propertyIds,
        &descriptions,
        &dataTypes
    );
    if (FAILED(hr)) {
        std::wcout << L"properties.query_error=" << HResultText(hr) << L"\n";
        return;
    }

    VARIANT* values = nullptr;
    HRESULT* errors = nullptr;
    HRESULT valuesHr = properties->GetItemProperties(
        const_cast<LPWSTR>(itemId.c_str()),
        count,
        propertyIds,
        &values,
        &errors
    );

    std::wcout << L"properties.supported=true properties.count=" << count
               << L" properties.values_result=" << HResultText(valuesHr) << L"\n";
    for (DWORD i = 0; i < count; ++i) {
        HRESULT itemError = errors ? errors[i] : valuesHr;
        std::wcout << L"  property.id=" << propertyIds[i]
                   << L" description=" << (descriptions && descriptions[i] ? descriptions[i] : L"")
                   << L" declared_type=" << VariantTypeText(dataTypes ? dataTypes[i] : VT_EMPTY)
                   << L" error=" << HResultText(itemError);
        if (SUCCEEDED(valuesHr) && values && SUCCEEDED(itemError)) {
            std::wcout << L" value=" << VariantText(values[i])
                       << L" value_type=" << VariantTypeText(values[i].vt);
        }
        std::wcout << L"\n";
    }

    if (values) {
        for (DWORD i = 0; i < count; ++i) {
            VariantClear(&values[i]);
        }
        CoTaskMemFree(values);
    }
    CoTaskMemFree(errors);
    if (descriptions) {
        for (DWORD i = 0; i < count; ++i) {
            CoTaskMemFree(descriptions[i]);
        }
        CoTaskMemFree(descriptions);
    }
    CoTaskMemFree(propertyIds);
    CoTaskMemFree(dataTypes);
}

} // namespace opcclient
