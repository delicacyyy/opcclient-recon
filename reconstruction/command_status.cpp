#include "commands.h"
#include "opc_client.h"

namespace opcclient {

void Status(const Options& options, const RuntimeSettings& settings) {
    const std::wstring& host = settings.host;
    std::wstring progId = Required(options, L"--progid", L"Kepware.KEPServerEX.V6");
    OpcSession session(host, settings.config.get());
    session.Connect(progId);
    OPCSERVERSTATUS* status = nullptr;
    Check(session.Server()->GetStatus(&status), L"IOPCServer::GetStatus");
    if (!status) {
        throw std::runtime_error("Server returned a null status structure");
    }
    std::wcout << L"server.progid=" << progId << L"\n"
               << L"server.clsid=" << GuidText(session.ServerClsid()) << L"\n"
               << L"server.state=" << ServerStateText(status->dwServerState) << L"\n"
               << L"server.vendor=" << (status->szVendorInfo ? status->szVendorInfo : L"") << L"\n"
               << L"server.version=" << status->wMajorVersion << L"." << status->wMinorVersion
               << L"." << status->wBuildNumber << L"\n"
               << L"server.groups=" << status->dwGroupCount << L"\n"
               << L"server.bandwidth=" << status->dwBandWidth << L"\n"
               << L"server.start_time=" << FileTimeText(status->ftStartTime) << L"\n"
               << L"server.current_time=" << FileTimeText(status->ftCurrentTime) << L"\n"
               << L"server.last_update_time=" << FileTimeText(status->ftLastUpdateTime) << L"\n";
    CoTaskMemFree(status->szVendorInfo);
    CoTaskMemFree(status);
}

} // namespace opcclient
