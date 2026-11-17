# Bench build helper (`make avr-bench`, gasguard_bench.hex, FMEA F49).
# Prints every line from the board with the PC time in milliseconds since the
# script started, and can send one bench command first:
#   #s  statistics line now: BENCH <uptime s> <longest cycle ms> <longest display ms> <free stack bytes>
#   #o  one cycle of 150 ms: one OVERRUN in the log
#   #f  three cycles of 150 ms in a row: OVERRUN x3, then FAULT 3 (REQ-SAF-008)
#   #h  the loop stops right after a watchdog kick: the watchdog resets the MCU (REQ-SAF-005)
# Watchdog timeout = (PC time of `BENCH BOOT`) - (PC time of `BENCH HANG`) - (ms printed by `BENCH BOOT`).
# Usage: .\tools\bench.ps1 -Port COM3 [-Command '#h'] [-Seconds 15]
# Opening the port does not reset the board (checked on the bench).
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Command = "",
    [int]$Seconds = 15,
    [int]$Baud = 38400
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$serial.ReadTimeout = 100
$serial.NewLine = "`r`n"

try {
    $serial.Open()
} catch {
    Write-Error "Cannot open ${Port}: $($_.Exception.Message). Close any other program using this port."
    exit 1
}

try {
    # A stray byte arrives when the port opens: a first newline flushes it (its answer is ERR).
    $serial.Write("`n")
    Start-Sleep -Milliseconds 500
    $serial.DiscardInBuffer()
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    if ($Command -ne "") {
        $serial.Write($Command)
        Write-Host ("{0,7} > {1}" -f $clock.ElapsedMilliseconds, $Command)
    }
    while ($clock.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $line = $serial.ReadLine()
            Write-Host ("{0,7}   {1}" -f $clock.ElapsedMilliseconds, $line)
        } catch [System.TimeoutException] {
            # No full line yet.
        }
    }
} finally {
    $serial.Close()
}
