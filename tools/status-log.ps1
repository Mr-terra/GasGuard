# Record the GasGuard `status` line at a fixed interval into a CSV file (REQ-LOG-005).
# Used to measure the temperature coefficient (REQ-FUN-004): run it for one to two
# days in clean air while the room warms and cools, then fit the gas raw values
# against the temperature raw value.
# Usage: .\tools\status-log.ps1 -Port COM3 [-IntervalMinutes 5] [-OutFile status-log.csv]
# Stop with Ctrl+C: the port is closed in the finally block.
# Opening the port does not reset the board (checked on the bench), so the
# detector keeps running while it is recorded.
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [int]$IntervalMinutes = 5,
    [string]$OutFile = "status-log.csv",
    [int]$Baud = 38400
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$serial.ReadTimeout = 2000
$serial.NewLine = "`r`n"

try {
    $serial.Open()
} catch {
    Write-Error "Cannot open ${Port}: $($_.Exception.Message). Close any other program using this port."
    exit 1
}

if (-not (Test-Path $OutFile)) {
    "time,state,conc,mq5_raw,mq2_raw,temp_raw,temp_valid" | Out-File -FilePath $OutFile -Encoding ascii
}
Write-Host "Recording the status line from $Port every $IntervalMinutes min into $OutFile. Press Ctrl+C to stop."

try {
    # A stray byte arrives when the port opens: a first newline flushes it (its answer is ERR).
    $serial.Write("`n")
    Start-Sleep -Seconds 1
    $serial.DiscardInBuffer()
    while ($true) {
        $serial.Write("status`n")
        $line = $null
        try {
            $line = $serial.ReadLine()
        } catch [System.TimeoutException] {
            # Empty on purpose: no answer is reported below.
        }
        $time = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        if (($null -ne $line) -and $line.StartsWith("STATUS ")) {
            $fields = $line.Substring(7).Trim() -split ' '
            "$time," + ($fields -join ',') | Out-File -FilePath $OutFile -Append -Encoding ascii
            Write-Host "$time  $line"
        } else {
            Write-Host "$time  no STATUS answer (got: $line)"
        }
        Start-Sleep -Seconds ($IntervalMinutes * 60)
        $serial.DiscardInBuffer()
    }
} finally {
    $serial.Close()
}
