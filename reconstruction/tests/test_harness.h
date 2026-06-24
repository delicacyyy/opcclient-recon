#pragma once

#include "../common.h"

#include <functional>

namespace opcclient::tests {

struct TestFailure final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct TestCase {
    const wchar_t* name;
    void (*run)();
};

inline void Fail(const std::string& message) {
    throw TestFailure(message);
}

inline void Require(bool condition, const char* message) {
    if (!condition) {
        Fail(message);
    }
}

template <typename Actual, typename Expected>
void RequireEqual(Actual actual, Expected expected, const char* message) {
    if (actual != expected) {
        std::ostringstream stream;
        stream << message
               << " actual=" << static_cast<unsigned long long>(actual)
               << " expected=" << static_cast<unsigned long long>(expected);
        Fail(stream.str());
    }
}

inline void RequireWideEqual(
    const std::wstring& actual,
    const std::wstring& expected,
    const char* message
) {
    if (actual != expected) {
        std::ostringstream stream;
        stream << message
               << " actual=" << opcclient::Narrow(actual)
               << " expected=" << opcclient::Narrow(expected);
        Fail(stream.str());
    }
}

inline void RequireSucceeded(HRESULT hr, const wchar_t* operation) {
    if (FAILED(hr)) {
        std::wstring message(operation);
        message += L" failed: ";
        message += opcclient::HResultText(hr);
        Fail(opcclient::Narrow(message));
    }
}

void TestConfigIdentityHardcodedCredentials();
void TestDiscoveryEnumeratesKepware();
void TestStatusKepwareRunning();
void TestBrowseReturnsItems();
void TestReadSyncReadsUi2Item();
void TestReadAsyncCompletesCallback();
void TestSubscribeSetupSucceeds();
void TestWriteSyncReadWriteRestore();
void TestWriteAsyncReadWriteRestore();
void TestWriteGuardCommandRejectsWithoutAllowWrite();

} // namespace opcclient::tests
