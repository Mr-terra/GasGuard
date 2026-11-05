# Flash GasGuard, then open the serial log on the same port.
# Usage: .\tools\flash-monitor.ps1 -Port COM3 [-Hex build\avr\gasguard.hex]
# If the flash fails, the log is not opened.
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Hex = "build\avr\gasguard.hex",
    [int]$Baud = 38400
)

& "$PSScriptRoot\flash.ps1" -Port $Port -Hex $Hex
if ($LASTEXITCODE -ne 0) {
    Write-Error "Flash failed (exit code $LASTEXITCODE). The log is not opened."
    exit $LASTEXITCODE
}

# The bootloader resets the board after the flash; the banner comes right after.
& "$PSScriptRoot\monitor.ps1" -Port $Port -Baud $Baud
