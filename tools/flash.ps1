# Flash GasGuard from Windows.
# Requires avrdude for Windows (e.g. https://github.com/avrdudes/avrdude/releases)
# Usage: .\tools\flash.ps1 -Port COM3 [-Hex build\avr\gasguard.hex] [-Bench]
# The simulation image (make avr-sim, gasguard_sim.hex) replaces the MQ-5 sensor by a
# potentiometer (FMEA F41). It is refused unless -Bench is given, for a bench test only.
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [string]$Hex = "build\avr\gasguard.hex",
    [switch]$Bench
)
if (-not (Test-Path $Hex)) {
    Write-Error "HEX file not found: $Hex. Run 'make avr' in WSL first."
    exit 1
}
if ((Split-Path $Hex -Leaf) -match 'sim' -and -not $Bench) {
    Write-Error "Simulation image refused: $Hex has no real gas sensor. Add -Bench for a bench test only (FMEA F41)."
    exit 1
}
# Arduino Uno R3: optiboot bootloader, 115200 baud
avrdude -p atmega328p -c arduino -P $Port -b 115200 -U "flash:w:${Hex}:i"
exit $LASTEXITCODE
