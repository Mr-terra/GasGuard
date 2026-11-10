# Read the GasGuard serial log from Windows (UART, 38400 baud, 8N1).
# Usage: .\tools\monitor.ps1 -Port COM3
# Stop with Ctrl+C: the port is closed in the finally block.
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [int]$Baud = 38400
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$serial.ReadTimeout = 500

try {
    $serial.Open()
} catch {
    Write-Error "Cannot open ${Port}: $($_.Exception.Message). Close any other program using this port."
    exit 1
}

Write-Host "Reading $Port at $Baud baud. Press Ctrl+C to stop."
try {
    while ($true) {
        try {
            Write-Host $serial.ReadLine()
        } catch [System.TimeoutException] {
            # Empty on purpose: a timeout only means no data yet.
        }
    }
} finally {
    $serial.Close()
}
