#pragma once

#include "test_harness.h"

#include "../opc_client.h"

namespace opcclient::tests {

extern const wchar_t kHost[];
extern const wchar_t kDomain[];
extern const wchar_t kUsername[];
extern const wchar_t kPassword[];
extern const wchar_t kPasswordPlaceholder[];
extern const wchar_t kProgId[];
extern const wchar_t kWritableItem[];
extern const USHORT kAlternateUi2Value;

struct Ui2ReadResult {
    USHORT value = 0;
    WORD quality = 0;
    FILETIME timestamp{};
};

bool PasswordPlaceholderStillPresent();
std::unique_ptr<ConnectionConfig> MakeLiveConfig();
std::unique_ptr<OpcSession> ConnectLiveSession(const ConnectionConfig* config);
Ui2ReadResult ReadUi2(IOPCServer* server, const ConnectionConfig* config);
void WriteUi2(IOPCServer* server, const ConnectionConfig* config, USHORT value);
void WriteUi2Async(IOPCServer* server, const ConnectionConfig* config, USHORT value);
USHORT ChooseTemporaryUi2Value(USHORT original);

} // namespace opcclient::tests
