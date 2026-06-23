[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$ScriptRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$Output = Join-Path $ScriptRoot 'artifacts\full-manifest-sha256.csv'
[System.IO.Directory]::CreateDirectory((Split-Path -Parent $Output)) | Out-Null

$rows = Get-ChildItem -LiteralPath $ScriptRoot -Recurse -File |
    Where-Object {
        $_.FullName -ne $Output -and
        $_.Extension -ne '.ini' -and
        $_.FullName -notmatch '\\reconstruction\\obj\\' -and
        $_.FullName -notmatch '\\ghidra\\project\\.*\.(db|gbf|lock)$'
    } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($ScriptRoot.Length).TrimStart([char[]]@('\', '/'))
        [pscustomobject]@{
            Path = $relative
            Length = $_.Length
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }

$rows | Export-Csv -LiteralPath $Output -NoTypeInformation -Encoding utf8
Write-Host "Manifest written: $Output ($($rows.Count) files)"
