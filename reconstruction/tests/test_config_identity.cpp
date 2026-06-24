#include "live_opc_fixture.h"

namespace opcclient::tests {

void TestConfigIdentityHardcodedCredentials() {
    auto config = MakeLiveConfig();
    RequireWideEqual(config->Host(), kHost, "host did not match");

    COAUTHIDENTITY identity = config->Identity();
    Require(identity.Flags == SEC_WINNT_AUTH_IDENTITY_UNICODE, "identity is not Unicode");
    RequireEqual(identity.DomainLength, static_cast<ULONG>(std::wstring(kDomain).size()), "domain length mismatch");
    RequireEqual(identity.UserLength, static_cast<ULONG>(std::wstring(kUsername).size()), "user length mismatch");
    RequireEqual(identity.PasswordLength, static_cast<ULONG>(std::wstring(kPassword).size()), "password length mismatch");
    Require(identity.Domain != nullptr, "domain pointer is null");
    Require(identity.User != nullptr, "user pointer is null");
    Require(identity.Password != nullptr, "password pointer is null");
    Require(reinterpret_cast<wchar_t*>(identity.Password)[identity.PasswordLength] == L'\0',
            "password buffer is not null-terminated");
}

} // namespace opcclient::tests
