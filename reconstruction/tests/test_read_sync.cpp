#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestReadSyncReadsUi2Item() {
    auto config = MakeLiveConfig();
    auto session = ConnectLiveSession(config.get());

    Ui2ReadResult result = ReadUi2(session->Server(), config.get());
    std::wcout << L"read.value=" << result.value
               << L" read.quality=" << QualityText(result.quality)
               << L" read.timestamp=" << FileTimeText(result.timestamp)
               << L"\n";
}

} // namespace opcclient::tests
