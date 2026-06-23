#include "commands.h"
#include "opc_client.h"

int wmain(int argc, wchar_t** argv) {
    using namespace opcclient;

    try {
        InitializeUtf8Io();
        Options options(argc, argv);
        if (options.Command().empty() || options.Command() == L"help" || options.Has(L"--help")) {
            PrintUsage();
            return 0;
        }

        ComRuntime runtime;
        bool interactive = options.Command() == L"interactive";
        RuntimeSettings settings = LoadRuntimeSettings(options, interactive);
        if (interactive) {
            Interactive(options, settings);
        } else if (options.Command() == L"discover") {
            Discover(options, settings);
        } else if (options.Command() == L"status") {
            Status(options, settings);
        } else if (options.Command() == L"browse") {
            Browse(options, settings);
        } else if (options.Command() == L"read") {
            Read(options, settings);
        } else if (options.Command() == L"subscribe") {
            Subscribe(options, settings);
        } else if (options.Command() == L"write") {
            Write(options, settings);
        } else {
            PrintUsage();
            return 2;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << "\n";
        return 1;
    }
}
