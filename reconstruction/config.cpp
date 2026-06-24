#include "config.h"

#include <fstream>

namespace opcclient {

ConnectionConfig::ConnectionConfig() = default;

ConnectionConfig::~ConnectionConfig() {
    ClearPassword();
}

std::unique_ptr<ConnectionConfig> ConnectionConfig::Load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open configuration file");
    }

    std::string bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }

    std::wstring text;
    try {
        text = WidenUtf8(bytes);
    } catch (...) {
        if (!bytes.empty()) {
            SecureZeroMemory(bytes.data(), bytes.size());
        }
        throw;
    }
    if (!bytes.empty()) {
        SecureZeroMemory(bytes.data(), bytes.size());
    }

    auto config = std::unique_ptr<ConnectionConfig>(new ConnectionConfig());
    std::set<std::wstring> seen;
    std::wstring section;
    std::wstring line;
    std::size_t lineNumber = 0;
    try {
        std::size_t position = 0;
        while (position <= text.size()) {
            std::size_t end = text.find(L'\n', position);
            if (end == std::wstring::npos) {
                end = text.size();
            }
            line.assign(text.data() + position, end - position);
            position = end == text.size() ? text.size() + 1 : end + 1;
            ++lineNumber;
            std::wstring trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == L';' || trimmed[0] == L'#') {
                continue;
            }
            if (trimmed.front() == L'[') {
                if (trimmed.back() != L']' || trimmed.size() < 3) {
                    throw std::runtime_error("Malformed INI section");
                }
                section = Trim(trimmed.substr(1, trimmed.size() - 2));
                if (section != L"server" && section != L"auth") {
                    throw std::runtime_error("Unknown INI section");
                }
                continue;
            }

            std::size_t equals = trimmed.find(L'=');
            if (equals == std::wstring::npos || section.empty()) {
                throw std::runtime_error("Malformed INI key/value");
            }
            std::wstring key = Trim(trimmed.substr(0, equals));
            std::wstring value = Trim(trimmed.substr(equals + 1));
            std::wstring qualified = section + L"." + key;
            if (!seen.insert(qualified).second) {
                throw std::runtime_error("Duplicate INI key");
            }

            if (qualified == L"server.ip") {
                config->host_ = value;
            } else if (qualified == L"auth.domain") {
                config->domain_ = value;
            } else if (qualified == L"auth.username") {
                config->username_ = value;
            } else if (qualified == L"auth.password") {
                config->passwordPresent_ = true;
                config->password_.assign(value.begin(), value.end());
                if (!config->password_.empty()) {
                    config->password_.push_back(L'\0');
                }
                if (!value.empty()) {
                    SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
                }
                if (!trimmed.empty()) {
                    SecureZeroMemory(trimmed.data(), trimmed.size() * sizeof(wchar_t));
                }
                if (!line.empty()) {
                    SecureZeroMemory(line.data(), line.size() * sizeof(wchar_t));
                }
            } else {
                throw std::runtime_error("Unknown INI key");
            }
        }
    } catch (const std::exception& error) {
        if (!text.empty()) {
            SecureZeroMemory(text.data(), text.size() * sizeof(wchar_t));
        }
        std::ostringstream message;
        message << "Invalid configuration near line " << lineNumber << ": " << error.what();
        throw std::runtime_error(message.str());
    }

    if (!text.empty()) {
        SecureZeroMemory(text.data(), text.size() * sizeof(wchar_t));
    }
    if (config->host_.empty()) {
        throw std::runtime_error("Configuration is missing server.ip");
    }
    if (config->username_.empty()) {
        throw std::runtime_error("Configuration is missing auth.username");
    }
    if (!config->passwordPresent_) {
        throw std::runtime_error("Configuration is missing auth.password");
    }
    return config;
}

std::unique_ptr<ConnectionConfig> ConnectionConfig::FromValues(
    std::wstring host,
    std::wstring domain,
    std::wstring username,
    std::wstring password
) {
    if (host.empty()) {
        throw std::runtime_error("Configuration is missing server.ip");
    }
    if (username.empty()) {
        throw std::runtime_error("Configuration is missing auth.username");
    }

    auto config = std::unique_ptr<ConnectionConfig>(new ConnectionConfig());
    config->host_ = std::move(host);
    config->domain_ = std::move(domain);
    config->username_ = std::move(username);
    config->passwordPresent_ = true;
    config->password_.assign(password.begin(), password.end());
    if (!config->password_.empty()) {
        config->password_.push_back(L'\0');
    }
    if (!password.empty()) {
        SecureZeroMemory(password.data(), password.size() * sizeof(wchar_t));
    }
    return config;
}

const std::wstring& ConnectionConfig::Host() const {
    return host_;
}

COAUTHIDENTITY ConnectionConfig::Identity() const {
    COAUTHIDENTITY identity{};
    identity.User = reinterpret_cast<USHORT*>(const_cast<wchar_t*>(username_.data()));
    identity.UserLength = static_cast<ULONG>(username_.size());
    identity.Domain = domain_.empty()
        ? nullptr
        : reinterpret_cast<USHORT*>(const_cast<wchar_t*>(domain_.data()));
    identity.DomainLength = static_cast<ULONG>(domain_.size());
    identity.Password = password_.empty()
        ? nullptr
        : reinterpret_cast<USHORT*>(const_cast<wchar_t*>(password_.data()));
    identity.PasswordLength = password_.empty()
        ? 0
        : static_cast<ULONG>(password_.size() - 1);
    identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
    return identity;
}

void ConnectionConfig::ClearPassword() {
    if (!password_.empty()) {
        SecureZeroMemory(password_.data(), password_.size() * sizeof(wchar_t));
        password_.clear();
        password_.shrink_to_fit();
    }
}

} // namespace opcclient
