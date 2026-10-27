# Flash GasGuard from Windows.
# Requires avrdude for Windows (e.g. https://github.com/avrdudes/avrdude/releases)
# Usage: .\tools\flash.ps1 -Port COM3 [-Hex build\avr\gasguard.hex]
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Hex = "build\avr\gasguard.hex"
)

if (-not (Test-Path $Hex)) {
    Write-Error "HEX file not found: $Hex. Run 'make avr' in WSL first."
    exit 1
}

# Arduino Uno R3: optiboot bootloader, 115200 baud
avrdude -p atmega328p -c arduino -P $Port -b 115200 -U "flash:w:${Hex}:i"
exit $LASTEXITCODE
