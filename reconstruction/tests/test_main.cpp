#include "pch.h"

#include "live_opc_fixture.h"

namespace opcclient::tests {
namespace {

const TestCase kTests[] = {
    {L"ConfigIdentity_HardcodedCredentials", TestConfigIdentityHardcodedCredentials},
    {L"Discovery_EnumeratesOpcDaServers", TestDiscoveryEnumeratesOpcDaServers},
    {L"Status_KepwareRunning", TestStatusKepwareRunning},
    {L"Browse_ReturnsItems", TestBrowseReturnsItems},
    {L"ReadSync_ReadsUi2Item", TestReadSyncReadsUi2Item},
    {L"ReadAsync_CompletesCallback", TestReadAsyncCompletesCallback},
    {L"Subscribe_SetupSucceeds", TestSubscribeSetupSucceeds},
    {L"WriteSync_ReadWriteRestore", TestWriteSyncReadWriteRestore},
    {L"WriteAsync_ReadWriteRestore", TestWriteAsyncReadWriteRestore},
    {L"WriteGuard_CommandRejectsWithoutAllowWrite", TestWriteGuardCommandRejectsWithoutAllowWrite},
};

void PrintAvailableTests() {
    std::wcout << L"Available tests:\n";
    for (const TestCase& test : kTests) {
        std::wcout << L"  " << test.name << L"\n";
    }
}

} // namespace
} // namespace opcclient::tests

int wmain(int argc, wchar_t** argv) {
    using namespace opcclient;
    using namespace opcclient::tests;

    try {
        InitializeUtf8Io();

        if (argc >= 2 && (std::wstring(argv[1]) == L"--help" || std::wstring(argv[1]) == L"help")) {
            PrintAvailableTests();
            return 0;
        }

        if (PasswordPlaceholderStillPresent()) {
            std::wcerr << L"ERROR: replace kPassword in reconstruction/tests/live_opc_fixture.cpp before running live tests.\n";
            return 2;
        }

        ComRuntime runtime;
        std::wstring selected = argc >= 2 ? argv[1] : L"";
        int selectedCount = 0;
        int passed = 0;
        int failed = 0;

        for (const TestCase& test : kTests) {
            if (!selected.empty() && selected != test.name) {
                continue;
            }
            ++selectedCount;

            auto start = std::chrono::steady_clock::now();
            std::wcout << L"[ RUN      ] " << test.name << L"\n";
            try {
                test.run();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                );
                ++passed;
                std::wcout << L"[       OK ] " << test.name << L" (" << elapsed.count() << L" ms)\n";
            } catch (const std::exception& error) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                );
                ++failed;
                std::wcerr << L"[  FAILED  ] " << test.name << L" (" << elapsed.count() << L" ms): "
                           << WidenUtf8(error.what()) << L"\n";
            }
        }

        if (selectedCount == 0) {
            std::wcerr << L"ERROR: unknown test name: " << selected << L"\n";
            PrintAvailableTests();
            return 2;
        }

        std::wcout << L"\nsummary.passed=" << passed
                   << L" summary.failed=" << failed
                   << L" summary.total=" << selectedCount << L"\n";
        return failed == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << "\n";
        return 1;
    }
}
