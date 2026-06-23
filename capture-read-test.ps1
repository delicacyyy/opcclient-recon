[CmdletBinding()]
param(
    [string]$Target = '',
    [string]$HostAddress = '169.254.1.3',
    [string]$ProgId = 'Kepware.KEPServerEX.V6',
    [string]$ItemId = '_System._Time',
    [int]$CaptureSeconds = 180,
    [string]$InterfaceAddress = '169.254.1.1',
    [switch]$LaunchClient
)

$ErrorActionPreference = 'Stop'
$ScriptRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$Runtime = Join-Path $ScriptRoot 'artifacts\runtime'
$Logs = Join-Path $ScriptRoot 'artifacts\logs'
[System.IO.Directory]::CreateDirectory($Runtime) | Out-Null
[System.IO.Directory]::CreateDirectory($Logs) | Out-Null
$Session = Get-Date -Format 'yyyyMMdd-HHmmss'
$Transcript = Join-Path $Logs "original-read-test-$Session.log"
$Pcap = Join-Path $Runtime "original-client-$Session.pcapng"

Start-Transcript -LiteralPath $Transcript
try {
    Write-Host "Read-only OPCClient.exe capture"
    Write-Host "Target: $Target"
    Write-Host "Host: $HostAddress"
    Write-Host "ProgID: $ProgId"
    Write-Host "Item: $ItemId"
    Write-Host "Writes are outside this procedure."

    $rpc = Test-NetConnection -ComputerName $HostAddress -Port 135 -InformationLevel Detailed
    $rpc | Format-List
    if (-not $rpc.TcpTestSucceeded) {
        throw "RPC endpoint mapper is not reachable at $HostAddress`:135"
    }

    $tshark = (Get-Command tshark.exe -ErrorAction Stop).Source
    $interfaces = & $tshark -D
    $interfaces | Out-File -LiteralPath (Join-Path $Runtime "tshark-interfaces-$Session.txt") -Encoding utf8
    $interfaceLine = $interfaces | Where-Object { $_ -match [regex]::Escape($InterfaceAddress) } | Select-Object -First 1
    if (-not $interfaceLine) {
        Write-Warning "Could not automatically find the interface containing $InterfaceAddress. Capture was not started."
    } else {
        $interfaceNumber = [regex]::Match($interfaceLine, '^\s*(\d+)\.').Groups[1].Value
        $captureArgs = @(
            '-i', $interfaceNumber,
            '-f', "host $HostAddress",
            '-a', "duration:$CaptureSeconds",
            '-w', $Pcap
        )
        Write-Host "Starting tshark capture on interface $interfaceNumber for at most $CaptureSeconds seconds."
        $capture = Start-Process -FilePath $tshark -ArgumentList $captureArgs -PassThru -WindowStyle Hidden
    }

    if ($LaunchClient) {
        if ([string]::IsNullOrWhiteSpace($Target)) {
            throw "No -Target supplied. Pass the path to the binary to launch (for example: -Target '.\bin\OPCClient.exe')."
        }
        if (-not (Test-Path -LiteralPath $Target -PathType Leaf)) {
            throw "Target not found: $Target"
        }
        Start-Process -FilePath $Target
    }

    @"

Manual UI sequence:
1. Open/select the remote server node dialog.
2. Enter $HostAddress.
3. Select $ProgId and connect.
4. Open server status and record the displayed fields.
5. Add item $ItemId to the default group.
6. Inspect item attributes/properties.
7. Perform a synchronous read.
8. Perform an asynchronous read and refresh.
9. Observe data changes for at least 10 seconds.
10. Remove the item and disconnect.

Do not invoke either write command.
"@ | Write-Host

    if ($capture) {
        Write-Host "Press Enter after completing the UI sequence; capture also stops automatically at its duration limit."
        [void](Read-Host)
        if (-not $capture.HasExited) {
            Stop-Process -Id $capture.Id
            $capture.WaitForExit()
        }
        Write-Host "Capture saved to $Pcap"
    }
} finally {
    Stop-Transcript
}
