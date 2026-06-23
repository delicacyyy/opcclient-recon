[CmdletBinding()]
param(
    [string]$HostAddress = '169.254.1.3',
    [string]$ProgId = 'Kepware.KEPServerEX.V6',
    [string]$ItemId = '_System._Time',
    [int]$SubscriptionSeconds = 5,
    [switch]$SkipAnalysis,
    [switch]$RequireCallbacks
)

$ErrorActionPreference = 'Stop'
$ScriptRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$Logs = Join-Path $ScriptRoot 'artifacts\logs'
$Runtime = Join-Path $ScriptRoot 'artifacts\runtime'
[System.IO.Directory]::CreateDirectory($Logs) | Out-Null
[System.IO.Directory]::CreateDirectory($Runtime) | Out-Null
$Session = Get-Date -Format 'yyyyMMdd-HHmmss'
$Log = Join-Path $Logs "verify-$Session.log"

Start-Transcript -LiteralPath $Log
try {
    if (-not $SkipAnalysis) {
        & (Join-Path $ScriptRoot 'analyze.ps1')
    }
    & (Join-Path $ScriptRoot 'build-reconstruction.ps1') -Configuration Both

    $exe = Join-Path $ScriptRoot 'reconstruction\bin\Release\opcclient-recon.exe'
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Release executable not found: $exe"
    }

    $tests = @(
        [pscustomobject]@{ Required = $true; Arguments = @('status', '--host', $HostAddress, '--progid', $ProgId) },
        [pscustomobject]@{ Required = $true; Arguments = @('read', '--host', $HostAddress, '--progid', $ProgId, '--item', $ItemId, '--mode', 'sync') },
        [pscustomobject]@{ Required = [bool]$RequireCallbacks; Arguments = @('read', '--host', $HostAddress, '--progid', $ProgId, '--item', $ItemId, '--mode', 'async') },
        [pscustomobject]@{ Required = [bool]$RequireCallbacks; Arguments = @('subscribe', '--host', $HostAddress, '--progid', $ProgId, '--item', $ItemId, '--seconds', $SubscriptionSeconds.ToString()) }
    )

    $callbackFailures = @()
    foreach ($test in $tests) {
        $args = $test.Arguments
        $name = ($args -join '-').Replace(':', '_').Replace('\', '_').Replace('/', '_')
        $output = Join-Path $Runtime "reconstruction-$Session-$name.txt"
        Write-Host "RUN: $exe $($args -join ' ')"
        $previousErrorAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & $exe @args 2>&1 | Tee-Object -FilePath $output
            $exit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorAction
        }
        if ($exit -ne 0 -and $test.Required) {
            throw "Verification command failed: $($args -join ' ')"
        }
        if ($exit -ne 0) {
            $callbackFailures += "$($args -join ' ') exited $exit"
            Write-Warning "Optional callback verification failed and was recorded: $($args -join ' ')"
        }
    }

    $required = @(
        'artifacts\static\metadata.json',
        'artifacts\maps\functions.csv',
        'artifacts\maps\callgraph.csv',
        'artifacts\maps\strings.csv',
        'artifacts\decompiled\raw\OPCClient_all.c',
        'artifacts\decompiled\annotated\OPCClient_all_annotated.c',
        'reconstruction\bin\Debug\opcclient-recon.exe',
        'reconstruction\bin\Release\opcclient-recon.exe'
    )
    foreach ($relative in $required) {
        $path = Join-Path $ScriptRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required artifact missing: $relative"
        }
    }
    if ($callbackFailures.Count) {
        $callbackFailures | Out-File -LiteralPath (Join-Path $Runtime "callback-limitations-$Session.txt") -Encoding utf8
        Write-Warning 'Verification completed with callback limitations. No write command was invoked.'
    } else {
        Write-Host 'Verification completed. No write command was invoked.'
    }
} finally {
    Stop-Transcript
}

& (Join-Path $ScriptRoot 'write-manifest.ps1')
