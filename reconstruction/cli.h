#pragma once

#include "config.h"

namespace opcclient {

class Options {
public:
    Options(int argc, wchar_t** argv);

    const std::wstring& Command() const;
    std::wstring Get(const std::wstring& key, const std::wstring& fallback = L"") const;
    bool Has(const std::wstring& key) const;
    int GetInt(const std::wstring& key, int fallback) const;

private:
    std::wstring command_;
    std::map<std::wstring, std::wstring> values_;
    std::map<std::wstring, bool> flags_;
    std::vector<std::wstring> positional_;
};

struct RuntimeSettings {
    std::unique_ptr<ConnectionConfig> config;
    std::wstring host;
};

void PrintUsage();
std::wstring Required(
    const Options& options,
    const std::wstring& key,
    const std::wstring& fallback = L""
);
RuntimeSettings LoadRuntimeSettings(const Options& options, bool configRequired = false);

} // namespace opcclient
