#pragma once

#include "common.h"

namespace opcclient {

class ConnectionConfig {
public:
    ConnectionConfig();
    ConnectionConfig(const ConnectionConfig&) = delete;
    ConnectionConfig& operator=(const ConnectionConfig&) = delete;
    ConnectionConfig(ConnectionConfig&&) = delete;
    ConnectionConfig& operator=(ConnectionConfig&&) = delete;
    ~ConnectionConfig();

    static std::unique_ptr<ConnectionConfig> Load(const std::filesystem::path& path);

    const std::wstring& Host() const;
    COAUTHIDENTITY Identity() const;

private:
    void ClearPassword();

    std::wstring host_;
    std::wstring domain_;
    std::wstring username_;
    std::vector<wchar_t> password_;
    bool passwordPresent_ = false;
};

} // namespace opcclient
