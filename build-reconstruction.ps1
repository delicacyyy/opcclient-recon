[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'Both')]
    [string]$Configuration = 'Both'
)

$ErrorActionPreference = 'Stop'
$ScriptRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$Project = Join-Path $ScriptRoot 'reconstruction\opcclient-recon.vcxproj'
$Logs = Join-Path $ScriptRoot 'artifacts\logs'
[System.IO.Directory]::CreateDirectory($Logs) | Out-Null
$Session = Get-Date -Format 'yyyyMMdd-HHmmss'
$Log = Join-Path $Logs "build-$Session.log"
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $VsWhere)) {
    throw "vswhere not found: $VsWhere"
}
$installation = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installation) {
    throw 'Visual Studio with MSBuild was not found.'
}
$MSBuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $MSBuild)) {
    throw "MSBuild not found: $MSBuild"
}

$configs = if ($Configuration -eq 'Both') { @('Debug', 'Release') } else { @($Configuration) }
foreach ($config in $configs) {
    "[$(Get-Date -Format o)] RUN: $MSBuild $Project /p:Configuration=$config /p:Platform=Win32" |
        Tee-Object -FilePath $Log -Append
    & $MSBuild $Project /m /t:Build "/p:Configuration=$config" '/p:Platform=Win32' /v:minimal 2>&1 |
        Tee-Object -FilePath $Log -Append
    if ($LASTEXITCODE -ne 0) {
        throw "$config Win32 build failed."
    }
}
Write-Host "Build completed: $($configs -join ', ') Win32"
