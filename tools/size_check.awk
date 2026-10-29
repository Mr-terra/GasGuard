# Checks avr-size output (berkeley format) against the memory budget (REQ-NFR-005).
# Usage: avr-size --format=berkeley firmware.elf | awk -v flash_max=26214 -v ram_max=1228 -f tools/size_check.awk
# Flash = .text + .data (data is copied from flash at boot); RAM = .data + .bss.
NR == 2 {
    flash = $1 + $2
    ram = $2 + $3
    printf "flash %d / %d bytes (80%% of 32 KB)\n", flash, flash_max
    printf "ram   %d / %d bytes (60%% of 2 KB)\n", ram, ram_max
    if (flash > flash_max || ram > ram_max) {
        print "REQ-NFR-005 budget exceeded"
        failed = 1
    }
}
END { exit failed }
