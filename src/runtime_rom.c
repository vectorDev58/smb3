#include "cyc_recomp.h"
#include <stddef.h>

const char *cyc_native_display_name = "Super Mario Bros. 3 - Remastered PC";
// Keep the same cartridge and save-state identity as the optional local recompile.
const char *cyc_native_program_name = "game";
const uint32_t cyc_native_prg_hash = 0x74f6e619u;
const uint32_t cyc_native_cart_hash = 0x04ec99a7u;
const uint8_t cyc_native_console = 1;
const uint32_t cyc_native_fds_bios_crc32 = 0;
const char *cyc_native_fds_bios_path = NULL;
const char *cyc_native_fds_image_path = NULL;
const char *cyc_native_fds_hle = NULL;
const CycRamView *const cyc_native_ram_views[] = {NULL};
const uint32_t cyc_native_ram_view_count = 0;
#include "runtime_hooks.inc"

bool cyc_native_has(uint16_t address) {
    (void)address;
    return false;
}

#if NESRECOMP_NATIVE_PRG_GUARDS
bool cyc_native_prg_guard_available(void) { return false; }
#endif

void cyc_native_run(void) {}

bool cyc_native_prg_code_byte(uint32_t offset) {
    (void)offset;
    return false;
}
