#include "pch.h"

#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestStatusKepwareRunning() {
    auto config = MakeLiveConfig();
    auto session = ConnectLiveSession(config.get());

    OPCSERVERSTATUS* status = nullptr;
    RequireSucceeded(session->Server()->GetStatus(&status), L"IOPCServer::GetStatus");
    Require(status != nullptr, "server returned null status");

    OPCSERVERSTATE state = status->dwServerState;
    std::wcout << L"server.state=" << ServerStateText(state) << L"\n";
    if (status->szVendorInfo) {
        std::wcout << L"server.vendor=" << status->szVendorInfo << L"\n";
    }

    CoTaskMemFree(status->szVendorInfo);
    CoTaskMemFree(status);

    RequireEqual(state, OPC_STATUS_RUNNING, "server is not running");
}

} // namespace opcclient::tests
