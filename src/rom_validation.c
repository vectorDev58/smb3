#include "cyc_core.h"
#include "rom_validation.h"
#include "sha256.h"
#include <stdio.h>
#include <string.h>

bool smb3_valid_rom(const uint8_t *image, size_t size) {
    if (size == SMB3_ROM_BYTES && memcmp(image, "NES\x1a", 4) == 0) {
        uint8_t digest[32];
        char hash[65];
        recompui_sha256_compute(image, size, digest);
        for (size_t i = 0; i < sizeof(digest); ++i)
            snprintf(hash + i * 2, 3, "%02x", (unsigned)digest[i]);
        return strcmp(hash, SMB3_ROM_SHA256) == 0;
    }
    return false;
}

bool smb3_load_ines(const uint8_t *image, size_t size) {
    if (smb3_valid_rom(image, size)) return cyc_load_ines(image, size);
    fprintf(stderr, "La ROM no coincide con Super Mario Bros. 3 (USA), PRG0. ROM does not match.\n");
    return false;
}
