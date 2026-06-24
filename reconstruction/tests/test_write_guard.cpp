#include "live_opc_fixture.h"

#include "../commands.h"

namespace opcclient::tests {

void TestWriteGuardCommandRejectsWithoutAllowWrite() {
    wchar_t executable[] = L"opcclient-recon-tests";
    wchar_t command[] = L"write";
    wchar_t hostKey[] = L"--host";
    wchar_t progIdKey[] = L"--progid";
    wchar_t itemKey[] = L"--item";
    wchar_t modeKey[] = L"--mode";
    wchar_t mode[] = L"sync";
    wchar_t typeKey[] = L"--type";
    wchar_t type[] = L"ui2";
    wchar_t valueKey[] = L"--value";
    wchar_t value[] = L"5";

    wchar_t* argv[] = {
        executable,
        command,
        hostKey,
        const_cast<wchar_t*>(kHost),
        progIdKey,
        const_cast<wchar_t*>(kProgId),
        itemKey,
        const_cast<wchar_t*>(kWritableItem),
        modeKey,
        mode,
        typeKey,
        type,
        valueKey,
        value
    };
    Options options(static_cast<int>(std::size(argv)), argv);
    RuntimeSettings settings;
    settings.config = MakeLiveConfig();
    settings.host = settings.config->Host();

    try {
        Write(options, settings);
    } catch (const std::exception& error) {
        std::string message = error.what();
        Require(message.find("Write rejected") != std::string::npos,
                "write failed for a reason other than the write guard");
        return;
    }

    Fail("write command did not reject missing --allow-write");
}

} // namespace opcclient::tests
