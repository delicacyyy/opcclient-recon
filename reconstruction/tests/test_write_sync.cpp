#include "pch.h"

#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestWriteSyncReadWriteRestore() {
    auto config = MakeLiveConfig();
    auto session = ConnectLiveSession(config.get());

    Ui2ReadResult original = ReadUi2(session->Server(), config.get());
    USHORT temporary = ChooseTemporaryUi2Value(original.value);
    bool restoreNeeded = false;

    std::wcout << L"write.original=" << original.value
               << L" write.temporary=" << temporary << L"\n";

    try {
        WriteUi2(session->Server(), config.get(), temporary);
        restoreNeeded = true;

        Ui2ReadResult changed = ReadUi2(session->Server(), config.get());
        RequireEqual(changed.value, temporary, "sync write did not change value");

        WriteUi2(session->Server(), config.get(), original.value);
        restoreNeeded = false;

        Ui2ReadResult restored = ReadUi2(session->Server(), config.get());
        RequireEqual(restored.value, original.value, "sync restore did not restore original value");
        std::wcout << L"write.restored=" << restored.value << L"\n";
    } catch (...) {
        if (restoreNeeded) {
            try {
                WriteUi2(session->Server(), config.get(), original.value);
            } catch (...) {
                std::wcerr << L"restore.after_failure=false\n";
            }
        }
        throw;
    }
}

} // namespace opcclient::tests
