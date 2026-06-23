# OPCClient.exe decompilation and reconstruction

This directory contains a reproducible, evidence-based analysis of the user-supplied `OPCClient.exe` binary (the target path is passed to `analyze.ps1` via `-Target` and is therefore not hardcoded here).

The work is limited to interoperability, documentation, and behavioral reconstruction. It does not claim recovery of FactorySoft's original source code. Ghidra output is C-like pseudocode inferred from native x86 machine code; names, local variables, types, comments, and source-file boundaries are mostly absent from the executable and therefore cannot be reproduced exactly.

## Current result

- Target SHA-256: `B9EAFE336F4DBF2AABF9AF0F402998AE8C5C1A19081361E485011B10282B965C`
- Product metadata: FactorySoft OPC Client 2.0, “OPC CLIENT COM Interface sample”
- PE format: unsigned, native Win32/x86 GUI executable
- Link timestamp: March 23, 1999 12:53:09
- Image base: `0x00400000`
- Entry point: `0x004092C0`
- Code size: 37,888 bytes
- Ghidra functions discovered: 491
- Ghidra functions successfully decompiled: 491
- Decompilation failures: 0
- External/import locations mapped: 85
- Raw per-function files: 491, plus one combined listing
- Annotated per-function files: 491, plus one combined listing
- Reconstruction: Win32 Debug and Release builds succeed
- Reconstruction source: command-oriented split; `main.cpp` is a 41-line dispatcher
- Runtime target: `Kepware.KEPServerEX.V6` at `169.254.1.3`
- Read item: `_System._Time`
- Synchronous read: successful
- OPC writes: not executed
- Asynchronous read callback: successful
- Subscription callback: successful, three data changes in the final two-second test

Earlier callback attempts returned `0x800706BA`; the final retry succeeded without changing Windows Firewall, DCOM, Kepware, OPCEnum, registry, or account policy. Both results remain in the logs.

## Directory map

```text
scripts/
  README.md                         this record
  analyze.ps1                       static analysis and headless decompilation
  build-reconstruction.ps1          Win32 Debug/Release builder
  capture-read-test.ps1             guided original-client capture procedure
  verify.ps1                        build plus read-only integration verification
  write-manifest.ps1                hashes the deliverable set
  ghidra/
    ExportOpcClient.java            headless Ghidra exporter
    annotations.csv                 reviewed semantic annotations
    project/                        reproducible local Ghidra project
  artifacts/
    static/                         PE metadata, imports, resources, and strings
    decompiled/raw/                 raw Ghidra pseudocode
    decompiled/annotated/           pseudocode with evidence and confidence notes
    maps/                           functions, calls, imports, strings, annotations
    runtime/                        reconstruction test output and limitations
    logs/                           exact tool/build/test session logs
    manifest-sha256.csv             static-analysis artifact hashes
    full-manifest-sha256.csv        complete deliverable hashes
  reconstruction/
    main.cpp                        UTF-8 setup, COM lifetime, and command dispatch
    common.h/.cpp                   COM pointer and formatting/UTF-8 utilities
    config.h/.cpp                   strict INI and credential handling
    opc_client.h/.cpp               DCOM activation, OPC sessions, groups, callbacks
    cli.h/.cpp                      option parsing, defaults, usage, runtime settings
    commands.h                      command entry-point declarations
    command_discover.cpp            OPCEnum DA 2.0 discovery
    command_status.cpp              server status query
    command_browse.cpp              noninteractive namespace browsing
    command_interactive.cpp         server selection, browsing, and interactive actions
    command_read.cpp                synchronous/asynchronous reads
    command_subscribe.cpp           timed data-change subscription
    command_write.cpp               guarded synchronous/asynchronous writes
    opc_guids.c                     OPC Foundation GUID definitions
    opcclient-recon.vcxproj         Win32 Visual C++ project
    bin/Debug/                      debug executable
    bin/Release/                    release executable
```

The most useful starting points are:

- `artifacts/decompiled/annotated/OPCClient_all_annotated.c`
- `artifacts/maps/functions.csv`
- `artifacts/maps/callgraph.csv`
- `artifacts/maps/imports.csv`
- `artifacts/maps/strings.csv`
- `reconstruction/main.cpp`
- `reconstruction/opc_client.h`
- `reconstruction/commands.h`

## Prerequisites

No tool installer is run by these scripts.

Place the following on `PATH` (or set the documented environment variable):

- **Ghidra 12 or newer.** `analyzeHeadless.bat` must resolve via `Get-Command`, or set `$env:GHIDRA_INSTALL_DIR` to the install root.
- **JDK 17 or newer.** Used only by Ghidra. `java.exe` and `javac.exe` must resolve via `Get-Command`; the script derives `JAVA_HOME` from the resolved `java.exe`.
- **Visual Studio with the C++ Win32 workload** and `vswhere.exe` discoverable at `${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe`.
- **OPC Foundation classic headers** for the reconstruction build. `opcclient-recon.vcxproj` looks for them at the OPC Foundation's default install path; override the `OpcInclude` MSBuild property to point elsewhere.
- **7-Zip (`7z.exe`)** for `analyze.ps1`.
- **Wireshark (`tshark.exe`)** for `capture-read-test.ps1`.
- **PowerShell 5.1** (ships with Windows).

`analyze.ps1` sets `JAVA_HOME`, `GHIDRA_INSTALL_DIR`, and prepends the JDK `bin` folder to `Path` only for its own process. It does not modify global or user environment variables, so stale values left in the interactive shell are harmless.

## Reproduce static analysis

From the workspace root:

```powershell
& .\scripts\analyze.ps1
```

The script performs these operations in order:

1. Resolves all output paths and refuses writes outside `scripts`.
2. Checks that the target exists.
3. Computes SHA-256 and aborts unless it matches the approved hash.
4. Records version metadata, timestamps, signature status, and file size.
5. Records the actual Java, Java compiler, and Ghidra versions.
6. Runs `dumpbin /headers`, `/imports`, and `/dependents`.
7. Runs `7z l -slt` for an independent PE inventory.
8. Extracts PE resources into `artifacts/static/resources`.
9. Extracts printable ASCII and UTF-16LE strings.
10. Imports the executable into a Ghidra project as `x86:LE:32:default:windows`.
11. Runs standard Ghidra auto-analysis with a 900-second ceiling.
12. Runs `ghidra/ExportOpcClient.java`.
13. Verifies that `functions.csv` was produced.
14. Hashes the generated static artifacts.

To analyze a deliberately different binary, its hash must be reviewed first:

```powershell
& .\scripts\analyze.ps1 `
  -Target 'C:\path\to\reviewed.exe' `
  -ExpectedSha256 'REVIEWED_SHA256'
```

`-AllowHashMismatch` exists for controlled analyst use, but should not be part of normal reproduction.

## What the Ghidra exporter records

For every discovered function:

- Original image address
- Ghidra-generated name
- Namespace and byte count
- Thunk and external status
- C-like decompilation result
- Direct call relationships
- Referenced import names and strings
- Suggested semantic name
- Confidence level
- Evidence supporting the suggestion

The exporter creates one file per function and a combined listing. Functions that cannot be decompiled are listed in `unresolved-functions.txt`; the present run has none.

Reviewed annotations currently include:

| Address | Suggested behavior | Confidence |
|---|---|---|
| `00404310` | Connect, initialize OPC, and create default group | High |
| `00404F00` | Add items and initialize item state | High |
| `00405470` | Enumerate group item attributes | High |
| `00405730` | Handle item write command | Medium |
| `00405B70` | Handle item read command | Medium |
| `00405FF0` | Refresh group data | High |
| `00406330` | Format OPC error | High |
| `004077F0` | Enumerate OPC servers | High |
| `00407FF0` | Resolve OPC server CLSID | High |

These are analytical labels, not recovered original symbol names.

## Static findings

The executable imports:

```text
MPR.dll
MFC42.dll
MSVCRT.dll
KERNEL32.dll
USER32.dll
ADVAPI32.dll
ole32.dll
OLEAUT32.dll
```

Important imported behavior includes:

- `CoCreateInstance` and `CoCreateInstanceEx`
- `CoInitializeSecurity`
- `CLSIDFromString`
- `CoTaskMemFree`
- Remote registry enumeration
- Windows network-resource enumeration
- MFC 4.2 GUI/runtime functions
- OLE Automation VARIANT management

Static strings and call paths establish:

- OPC Classic COM/DCOM, not OPC UA
- Explicit OPC 2.0 Server Browser support
- Local and remote server selection
- OPCEnum with remote-registry fallback
- `IOPCServer::AddGroup`
- Group naming/state management
- Item addition/removal and attribute enumeration
- Synchronous and asynchronous read/write paths
- `Refresh` and `Refresh2`
- OPC DA 1.0-style `IDataObject::DAdvise` notification support
- Server status, item properties, quality, timestamps, and shutdown handling

The original client therefore appears primarily targeted at OPC DA 2.0 while retaining some OPC DA 1.0 notification compatibility.

## Build the reconstruction

```powershell
& .\scripts\build-reconstruction.ps1 -Configuration Both
```

The build script:

1. Uses `vswhere.exe` to locate the active Visual Studio installation.
2. Locates MSBuild without relying on a Developer Command Prompt.
3. Builds `Debug|Win32`.
4. Builds `Release|Win32`.
5. Records every command and result under `artifacts/logs`.

The reconstruction uses only public Windows COM and OPC Foundation interfaces. It is not generated by copying pseudocode back into a project.

The source is deliberately command-oriented:

- Shared platform, UTF-8, COM-pointer, and formatting code is in `common.h/.cpp`.
- Configuration and credential lifetime are isolated in `config.h/.cpp`.
- DCOM authentication, OPC sessions/groups, callbacks, proxy blankets, and shared item output are in `opc_client.h/.cpp`.
- CLI parsing and runtime configuration are in `cli.h/.cpp`.
- Each public command has its own `command_*.cpp`; command-only helpers remain private to that translation unit.
- `main.cpp` performs initialization and dispatch only. No `.cpp` file includes another `.cpp` file.

Implemented commands:

```text
opcclient-recon interactive --config FILE
opcclient-recon interactive --config .\opcclient.ini.example
opcclient-recon discover --host HOST [--limit N]
opcclient-recon discover --host 169.254.1.3
opcclient-recon status --host HOST --progid PROGID
opcclient-recon browse --host HOST --progid PROGID [--limit N]
opcclient-recon read --host HOST --progid PROGID --item ITEM --mode sync|async
opcclient-recon subscribe --host HOST --progid PROGID --item ITEM --seconds N
opcclient-recon write --host HOST --progid PROGID --item ITEM \
  --mode sync|async --type TYPE --value VALUE --allow-write
```

The write command is intentionally guarded by `--allow-write`. No supplied verification script includes that option, and no write was performed during this work.

## Runtime discovery with explicit DCOM credentials

The reconstruction supports a strict UTF-8 INI file containing only the remote address and authentication data:

```ini
[server]
ip=169.254.1.3

[auth]
domain=
username=opcuser
password=
```

`auth.domain` is optional and may also be intentionally empty as `domain=`; both forms produce a null domain pointer with a zero length in `COAUTHIDENTITY`. The `server.ip`, `auth.username`, and `auth.password` keys are required. `auth.password` may be intentionally empty as `password=`, but omitting the key is an error. Unknown sections, unknown keys, duplicate keys, malformed lines, and empty IP or username values are rejected. A template is available at `reconstruction/opcclient.ini.example`.

Start runtime discovery with:

```powershell
.\scripts\reconstruction\bin\Release\opcclient-recon.exe interactive `
  --config C:\secure\opcclient.ini
```

Interactive mode:

1. Activates OPCEnum using the configured IP and credentials.
2. Enumerates OPC DA 3.0, 2.0, and 1.0 categories.
3. Deduplicates servers by CLSID and displays ProgID, user type, CLSID, and categories.
4. Prompts for a server instead of storing a ProgID in configuration.
5. Displays server status and supported server interfaces.
6. Browses hierarchical or flat namespaces with paging and branch navigation.
7. Filters for readable items where supported and prompts for an item.
8. Offers synchronous read, asynchronous read, timed subscription, another item, another server, or exit.

The same `--config` option can be used with existing noninteractive commands. In that case `server.ip` replaces `--host`, while ProgID, item, mode, duration, and write values remain command-line/runtime choices.

Explicit credentials are supplied to `CoCreateInstanceEx` through `COAUTHINFO`/`COAUTHIDENTITY` and applied with `CoSetProxyBlanket` to OPCEnum, server enumerators, server interfaces, namespace and property interfaces, group interfaces, I/O interfaces, and connection-point proxies. Packet privacy and impersonation-level impersonation are requested.

The password is held in mutable memory and wiped with `SecureZeroMemory` when configuration is destroyed. It is never printed by the client. Nevertheless, the INI itself is plaintext:

- Store it outside the workspace where possible.
- Restrict its NTFS ACL to the service/operator account.
- Do not pass the password on the command line.
- Do not attach it to support logs.
- `scripts/.gitignore` rejects `*.ini`.
- `write-manifest.ps1` excludes all files whose extension is `.ini`.

The explicit credential path was verified against OPCEnum at `169.254.1.3` with an empty domain and empty password. Both `discover --config` and config-driven interactive discovery returned the same three OPC servers. No server was selected and no item read, subscription, or write was performed during this credential verification.

Interactive output is initialized as UTF-8 before argument processing. The program sets UTF-8 console code pages for terminal handles and gives the wide input/output streams UTF-8 character-conversion facets while retaining classic numeric formatting. This prevents Unicode punctuation or OPC-provided names from setting `std::wcout`'s failure state. Redirected output remains UTF-8.

The Unicode fix was verified in Win32 Debug and Release builds. A live interactive discovery displayed all three server rows and its prompt in a terminal. A raw redirected-output capture was strict-valid UTF-8 and contained all three rows, a visible prompt, and nine em dashes represented by nine `E2 80 94` byte sequences.

## Reproduce the read-only runtime verification

```powershell
& .\scripts\verify.ps1 -SkipAnalysis -SubscriptionSeconds 5
```

Omit `-SkipAnalysis` to rerun static analysis first.

The fixed test values are:

```text
Host:    169.254.1.3
ProgID:  Kepware.KEPServerEX.V6
Item:    _System._Time
Account: current logged-in Windows identity
```

Mandatory verification:

1. Build Debug and Release Win32.
2. Activate OPCEnum remotely.
3. Resolve the server ProgID remotely.
4. Activate Kepware through DCOM.
5. Read server status.
6. Query `_System._Time` properties.
7. Add a group and item.
8. Perform a synchronous device read.
9. Remove the item and group.

Optional callback verification:

1. Advise `IOPCDataCallback`.
2. Perform `IOPCAsyncIO2::Read`.
3. Observe subscriptions.

Use `-RequireCallbacks` if callback failure should make verification fail.

## Observed runtime values

OPCEnum discovery returned:

```text
KEPware.KEPServerEx.V4
Kepware.KEPServerEX.V6
Matrikon.OPC.Simulation.1
```

Kepware status:

```text
CLSID:  {7BC0CC8E-482C-47CA-ABDC-0FE7F9C6E729}
State:  RUNNING
Vendor: Kepware
Version: 6.5.829
```

`_System._Time`:

```text
Canonical type: VT_BSTR
Access rights:  0x1 (read-only)
Quality:        Good, 0x00C0
Server scan rate property: 10 ms
```

Eight item properties were returned:

```text
1     Item Canonical Data Type
2     Item Value
3     Item Quality
4     Item Timestamp
5     Item Access Rights
6     Server Scan Rate
101   Item Description
5003  DDE Access Name
```

Example successful synchronous read:

```text
value=12:02:03 PM
vartype=VT_BSTR
quality=Good (0x00C0)
timestamp=2026-06-23T04:02:02.871Z
```

Values and timestamps naturally differ on later runs.

## Callback verification and transient failure

Initial asynchronous read and subscription attempts reached group/item creation, then failed during `IConnectionPoint::Advise`:

```text
0x800706BA: The RPC server is unavailable.
```

No configuration was changed. A later end-to-end verification established the callback channel successfully:

```text
OnDataChange: successful
IOPCAsyncIO2::Read immediate result: successful
OnReadComplete transaction 1001: successful
Two-second subscription: three OnDataChange item deliveries
```

The earlier failure was therefore transient, plausibly involving dynamic RPC endpoint readiness or timing rather than a persistent policy block. The failed and successful runs are both retained so a reproducer does not mistake intermittent callback setup for deterministic behavior.

`verify.ps1 -RequireCallbacks` can be used when callbacks must be treated as mandatory.

## Guided capture of the original GUI

The original client is interactive, so its full runtime sequence requires a human operator:

```powershell
& .\scripts\capture-read-test.ps1 -LaunchClient
```

The script:

- Confirms TCP 135 reachability.
- Selects the tshark interface containing `169.254.1.1`.
- Captures only traffic involving `169.254.1.3`.
- Launches the original client only when `-LaunchClient` is supplied.
- Prints the exact UI sequence.
- Explicitly prohibits write commands.
- Saves the transcript and packet capture under `artifacts`.

The guided original-client UI sequence was not executed automatically in this implementation session because no safe desktop automation interface was available. The script is ready for an analyst to perform and records the resulting evidence.

## Implementation/debugging journal

Detailed timestamps and raw command output are preserved in `artifacts/logs`. The material implementation steps were:

1. Read `.agents\PLANS.md`.
2. Confirmed the authorized target path.
3. Collected target metadata, hash, signature, and adjacent installation context.
4. Identified the client as native x86 MFC rather than .NET.
5. Extracted embedded strings proving OPC 2.0, COM/DCOM, sync/async I/O, groups, items, properties, and notifications.
6. Confirmed the remote lab route from `169.254.1.1` to `169.254.1.3`.
7. Confirmed TCP 135 was reachable.
8. Located Ghidra 12.1.2 and a Temurin JDK 25.0.3 LTS installation.
9. Found stale shell environment variables and chose process-local tool resolution.
10. Located Visual Studio Win32 compiler/MFC support and OPC Foundation headers.
11. Created the Ghidra exporter and PowerShell orchestration.
12. Created the public-interface console reconstruction.
13. First build failed because a COM interface does not have a virtual destructor; removed the invalid C++ `override`.
14. Fixed UTF-16-to-UTF-8 exception formatting.
15. Debug Win32 build succeeded.
16. Remote OPCEnum discovery and Kepware status succeeded.
17. Synchronous `_System._Time` read succeeded.
18. Initial async/subscription `Advise` returned `0x800706BA`; recorded without policy changes.
19. First analysis run exposed a PowerShell interpolation error around an exit-code colon; corrected it.
20. Second analysis attempt exposed PowerShell 5.1 handling of native stderr; invocation now records it without treating exit-code zero as failure.
21. First Ghidra exporter compilation used the wrong package for `DefinedDataIterator`; corrected it to `ghidra.program.util`.
22. PowerShell 5.1 lacked `.NET Path.GetRelativePath`; replaced it with a compatible normalized substring.
23. Ghidra then exported all 491 functions with no failures.
24. Added a complete external-import map.
25. Added reviewed semantic annotations.
26. Added OPC item property querying to the reconstruction.
27. Debug and Release Win32 builds succeeded.
28. Final verification confirmed status, properties, synchronous read, asynchronous read, subscription callbacks, and cleanup.
29. The final callback retry succeeded without any security or network configuration change.
30. Confirmed that no write operation was invoked.
31. Added explicit empty-domain support: an omitted `auth.domain` key or `domain=` now yields a null `COAUTHIDENTITY::Domain` pointer and zero `DomainLength`.
32. Updated the example configuration and documentation to demonstrate an empty domain.
33. Compiled the empty-domain change in Win32 Debug and Release configurations; OPC/DCOM runtime tests were intentionally skipped.
34. Traced disappearing interactive discovery output to `std::wcout` using the C runtime's default `C` locale: writing the Unicode em dash set the stream failure state and suppressed all subsequent rows and prompts.
35. Added process-local UTF-8 console and wide-stream initialization so Unicode separators and OPC-provided names remain valid in terminals, pipes, and redirected files.
36. Built the UTF-8 change successfully in Win32 Debug and Release configurations.
37. Re-ran current-identity DA 2.0 discovery and confirmed the same three servers were returned.
38. Re-ran config-driven interactive discovery, confirmed all three DA 1.0/2.0/3.0 rows and the prompt were visible, and exited without selecting a server.
39. Captured raw redirected interactive output through `cmd.exe`; strict UTF-8 decoding succeeded and all nine em dashes used the expected `E2 80 94` encoding.
40. Mapped the 2,135-line monolithic reconstruction into shared platform, configuration, OPC, CLI, and command-specific dependency layers.
41. Split the reconstruction into four shared header/source pairs, one command declaration header, seven command implementation files, and a 41-line `main.cpp`.
42. Moved command-only helpers into anonymous namespaces: interactive discovery/browsing/menu helpers remain in `command_interactive.cpp`, and write-value parsing remains in `command_write.cpp`.
43. Updated `opcclient-recon.vcxproj` to compile all new translation units and expose all new headers in Visual Studio.
44. Built the split source successfully in Win32 Debug and Release configurations without compiler or linker errors.
45. Ran `help` and confirmed the command syntax, defaults, credential description, and write guard were unchanged.
46. Ran `discover --host 169.254.1.3`; OPCEnum returned the same KEPware V4, Kepware V6, and Matrikon Simulation DA 2.0 servers.
47. Preserved a user-edited `opcclient.ini.example` that pointed at another environment instead of overwriting it for testing.
48. Created a temporary ignored INI for `169.254.1.3` with an empty domain and password, ran interactive discovery through a raw `cmd.exe` input pipe, and removed the INI immediately afterward.
49. The post-refactor interactive capture exited successfully, decoded as strict UTF-8, displayed three server rows and nine em dashes, showed the selection prompt, and never selected or activated a server.
50. No tag browse, item read, subscription, or write operation was performed during the source-split verification.

## Integrity

Generate or refresh the full deliverable manifest:

```powershell
& .\scripts\write-manifest.ps1
```

The manifest excludes compiler intermediate objects and Ghidra database cache internals, but includes source, scripts, reports, decompiled output, logs, runtime evidence, and final executables.

## Safety notes

- Use only systems and binaries covered by written authorization.
- Keep captures and decompiled output internal.
- Do not upload the binary or captures to public services.
- Do not interpret pseudocode as original copyrighted source text.
- Do not execute `write` without separately approved tags and values.
- Do not lower firewall, DCOM, authentication, or account protections merely to make a test pass.
