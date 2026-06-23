#include "cli.h"

namespace opcclient {

Options::Options(int argc, wchar_t** argv) {
    if (argc >= 2) {
        command_ = argv[1];
    }
    for (int i = 2; i < argc; ++i) {
        std::wstring key = argv[i];
        if (key.rfind(L"--", 0) != 0) {
            positional_.push_back(key);
            continue;
        }
        if (i + 1 < argc && std::wstring(argv[i + 1]).rfind(L"--", 0) != 0) {
            values_[key] = argv[++i];
        } else {
            flags_[key] = true;
        }
    }
}

const std::wstring& Options::Command() const {
    return command_;
}

std::wstring Options::Get(const std::wstring& key, const std::wstring& fallback) const {
    auto iterator = values_.find(key);
    return iterator == values_.end() ? fallback : iterator->second;
}

bool Options::Has(const std::wstring& key) const {
    return flags_.contains(key) || values_.contains(key);
}

int Options::GetInt(const std::wstring& key, int fallback) const {
    std::wstring value = Get(key);
    return value.empty() ? fallback : std::stoi(value);
}

void PrintUsage() {
    std::wcout <<
        L"OPC DA console reconstruction for FactorySoft OPCClient behavior\n\n"
        L"Commands:\n"
        L"  opcclient-recon interactive --config FILE\n"
        L"  opcclient-recon discover --host HOST [--limit N]\n"
        L"  opcclient-recon status --host HOST --progid PROGID\n"
        L"  opcclient-recon browse --host HOST --progid PROGID [--limit N]\n"
        L"  opcclient-recon read --host HOST --progid PROGID --item ITEM --mode sync|async\n"
        L"  opcclient-recon subscribe --host HOST --progid PROGID --item ITEM [--seconds N]\n"
        L"  opcclient-recon write --host HOST --progid PROGID --item ITEM --mode sync|async\n"
        L"                         --type bool|i2|i4|ui2|ui4|r4|r8|string --value VALUE --allow-write\n\n"
        L"Connection configuration:\n"
        L"  --config FILE supplies server.ip plus auth.domain/username/password from a UTF-8 INI file.\n"
        L"  When --config is present, server.ip replaces --host and credentials are applied to DCOM.\n\n"
        L"Defaults: host=169.254.1.3, progid=Kepware.KEPServerEX.V6, item=_System._Time\n"
        L"Write is compiled but rejected unless --allow-write is explicitly provided.\n";
}

std::wstring Required(
    const Options& options,
    const std::wstring& key,
    const std::wstring& fallback
) {
    std::wstring value = options.Get(key, fallback);
    if (value.empty()) {
        throw std::runtime_error("Missing required option");
    }
    return value;
}

RuntimeSettings LoadRuntimeSettings(const Options& options, bool configRequired) {
    RuntimeSettings settings;
    std::wstring configPath = options.Get(L"--config");
    if (!configPath.empty()) {
        settings.config = ConnectionConfig::Load(std::filesystem::path(configPath));
        settings.host = settings.config->Host();
        std::wcout << L"config.loaded=true auth.mode=explicit host=" << settings.host << L"\n";
    } else {
        if (configRequired) {
            throw std::runtime_error("--config is required for interactive mode");
        }
        settings.host = Required(options, L"--host", L"169.254.1.3");
    }
    return settings;
}

} // namespace opcclient
