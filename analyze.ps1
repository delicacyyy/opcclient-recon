[CmdletBinding()]
param(
    [string]$Target = '',
    [string]$ExpectedSha256 = 'B9EAFE336F4DBF2AABF9AF0F402998AE8C5C1A19081361E485011B10282B965C',
    [switch]$AllowHashMismatch
)

$ErrorActionPreference = 'Stop'
$ScriptRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$Artifacts = Join-Path $ScriptRoot 'artifacts'
$Static = Join-Path $Artifacts 'static'
$Maps = Join-Path $Artifacts 'maps'
$Logs = Join-Path $Artifacts 'logs'
$ProjectRoot = Join-Path $ScriptRoot 'ghidra\project'
$Session = Get-Date -Format 'yyyyMMdd-HHmmss'
$LogFile = Join-Path $Logs "analyze-$Session.log"

function Ensure-Directory([string]$Path) {
    [System.IO.Directory]::CreateDirectory($Path) | Out-Null
}

function Assert-UnderScripts([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($ScriptRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to write outside scripts: $full"
    }
}

function Reset-SafeDirectory([string]$Path) {
    Assert-UnderScripts $Path
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    Ensure-Directory $Path
}

function Invoke-Logged {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter()][string[]]$ArgumentList = @(),
        [Parameter()][string]$OutputPath
    )
    $quoted = $ArgumentList | ForEach-Object {
        if ($_ -match '\s') { '"{0}"' -f ($_ -replace '"', '\"') } else { $_ }
    }
    "[$(Get-Date -Format o)] RUN: $FilePath $($quoted -join ' ')" | Tee-Object -FilePath $LogFile -Append
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $FilePath @ArgumentList 2>&1
        $exit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    $output | Tee-Object -FilePath $LogFile -Append
    if ($OutputPath) {
        $output | Out-File -LiteralPath $OutputPath -Encoding utf8
    }
    "[$(Get-Date -Format o)] EXIT: $exit" | Tee-Object -FilePath $LogFile -Append
    if ($exit -ne 0) {
        throw "Command failed with exit code ${exit}: $FilePath"
    }
}

foreach ($directory in @($Artifacts, $Static, $Maps, $Logs, $ProjectRoot)) {
    Ensure-Directory $directory
}
Reset-SafeDirectory (Join-Path $Artifacts 'decompiled\raw')
Reset-SafeDirectory (Join-Path $Artifacts 'decompiled\annotated')
Reset-SafeDirectory $Maps
Reset-SafeDirectory (Join-Path $Static 'resources')

if ([string]::IsNullOrWhiteSpace($Target)) {
    throw "No -Target supplied. Pass the path to the binary to analyze (for example: -Target '.\bin\OPCClient.exe')."
}
if (-not (Test-Path -LiteralPath $Target -PathType Leaf)) {
    throw "Target not found: $Target"
}

$hash = (Get-FileHash -LiteralPath $Target -Algorithm SHA256).Hash.ToUpperInvariant()
if ($hash -ne $ExpectedSha256.ToUpperInvariant() -and -not $AllowHashMismatch) {
    throw "SHA-256 mismatch. Expected $ExpectedSha256 but found $hash. Use -AllowHashMismatch only after reviewing the target."
}

$Java = (Get-Command java.exe -ErrorAction Stop).Source
$Javac = (Get-Command javac.exe -ErrorAction Stop).Source
$env:JAVA_HOME = [System.IO.Path]::GetFullPath((Split-Path -Parent (Split-Path -Parent $Java)))
if ($env:GHIDRA_INSTALL_DIR) {
    $env:GHIDRA_INSTALL_DIR = [System.IO.Path]::GetFullPath($env:GHIDRA_INSTALL_DIR)
    $Headless = Join-Path $env:GHIDRA_INSTALL_DIR 'support\analyzeHeadless.bat'
} else {
    $Headless = (Get-Command analyzeHeadless.bat -ErrorAction Stop).Source
    $env:GHIDRA_INSTALL_DIR = [System.IO.Path]::GetFullPath((Split-Path -Parent (Split-Path -Parent $Headless)))
}
$Dumpbin = Get-ChildItem -LiteralPath (Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio') -Recurse -Filter dumpbin.exe -File |
    Where-Object { $_.FullName -match 'Hostx64\\x86\\dumpbin\.exe$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
$SevenZip = (Get-Command 7z.exe -ErrorAction Stop).Source
$env:Path = "$(Join-Path $env:JAVA_HOME 'bin');$env:Path"

foreach ($tool in @($Java, $Javac, $Headless, $Dumpbin, $SevenZip)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required tool not found: $tool"
    }
}

"[$(Get-Date -Format o)] Target: $Target" | Out-File -LiteralPath $LogFile -Encoding utf8
"[$(Get-Date -Format o)] SHA256: $hash" | Out-File -LiteralPath $LogFile -Encoding utf8 -Append

$file = Get-Item -LiteralPath $Target
$version = $file.VersionInfo
$signature = Get-AuthenticodeSignature -LiteralPath $Target
$metadata = [ordered]@{
    target = $Target
    sha256 = $hash
    length = $file.Length
    creation_time_utc = $file.CreationTimeUtc.ToString('o')
    last_write_time_utc = $file.LastWriteTimeUtc.ToString('o')
    file_version = $version.FileVersion
    product_version = $version.ProductVersion
    company = $version.CompanyName
    product = $version.ProductName
    description = $version.FileDescription
    original_filename = $version.OriginalFilename
    signature_status = $signature.Status.ToString()
    analyzed_at = (Get-Date).ToString('o')
}
$metadata | ConvertTo-Json | Out-File -LiteralPath (Join-Path $Static 'metadata.json') -Encoding utf8

Invoke-Logged -FilePath $Java -ArgumentList @('-version') -OutputPath (Join-Path $Static 'java-version.txt')
Invoke-Logged -FilePath $Javac -ArgumentList @('-version') -OutputPath (Join-Path $Static 'javac-version.txt')
Invoke-Logged -FilePath $Dumpbin -ArgumentList @('/headers', $Target) -OutputPath (Join-Path $Static 'dumpbin-headers.txt')
Invoke-Logged -FilePath $Dumpbin -ArgumentList @('/imports', $Target) -OutputPath (Join-Path $Static 'dumpbin-imports.txt')
Invoke-Logged -FilePath $Dumpbin -ArgumentList @('/dependents', $Target) -OutputPath (Join-Path $Static 'dumpbin-dependents.txt')
Invoke-Logged -FilePath $SevenZip -ArgumentList @('l', '-slt', $Target) -OutputPath (Join-Path $Static '7zip-pe-listing.txt')
Invoke-Logged -FilePath $SevenZip -ArgumentList @('x', '-y', "-o$(Join-Path $Static 'resources')", $Target) -OutputPath (Join-Path $Static '7zip-resource-extraction.txt')

$bytes = [System.IO.File]::ReadAllBytes($Target)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
$unicode = [System.Text.Encoding]::Unicode.GetString($bytes)
[regex]::Matches($ascii, '[ -~]{4,}') | ForEach-Object Value | Sort-Object -Unique |
    Out-File -LiteralPath (Join-Path $Static 'strings-ascii.txt') -Encoding utf8
[regex]::Matches($unicode, '[ -~]{4,}') | ForEach-Object Value | Sort-Object -Unique |
    Out-File -LiteralPath (Join-Path $Static 'strings-utf16le.txt') -Encoding utf8

$applicationProperties = Join-Path $env:GHIDRA_INSTALL_DIR 'Ghidra\application.properties'
Get-Content -LiteralPath $applicationProperties |
    Where-Object { $_ -match '^application\.(version|release\.name|build\.date)=' } |
    Out-File -LiteralPath (Join-Path $Static 'ghidra-version.txt') -Encoding utf8

$annotationCsv = Join-Path $ScriptRoot 'ghidra\annotations.csv'
$ghidraScriptPath = Join-Path $ScriptRoot 'ghidra'
$headlessArgs = @(
    $ProjectRoot,
    'OPCClientAnalysis',
    '-import',
    $Target,
    '-overwrite',
    '-analysisTimeoutPerFile',
    '900',
    '-scriptPath',
    $ghidraScriptPath,
    '-postScript',
    'ExportOpcClient.java',
    $Artifacts,
    $annotationCsv,
    '-log',
    (Join-Path $Logs "ghidra-$Session.log"),
    '-scriptlog',
    (Join-Path $Logs "ghidra-script-$Session.log")
)
Invoke-Logged -FilePath $Headless -ArgumentList $headlessArgs

if (-not (Test-Path -LiteralPath (Join-Path $Maps 'functions.csv') -PathType Leaf)) {
    throw "Ghidra completed without producing functions.csv. Review the Ghidra logs under $Logs."
}

$manifest = Get-ChildItem -LiteralPath $Artifacts -Recurse -File |
    Where-Object { $_.FullName -ne $LogFile -and $_.Name -ne 'manifest-sha256.csv' } |
    Sort-Object FullName |
    ForEach-Object {
        $relativePath = $_.FullName.Substring($ScriptRoot.Length).TrimStart([char[]]@('\', '/'))
        [pscustomobject]@{
            Path = $relativePath
            Length = $_.Length
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }
$manifest | Export-Csv -LiteralPath (Join-Path $Artifacts 'manifest-sha256.csv') -NoTypeInformation -Encoding utf8

"[$(Get-Date -Format o)] Static analysis completed." | Tee-Object -FilePath $LogFile -Append
Write-Host "Static analysis complete. Artifacts: $Artifacts"
