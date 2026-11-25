#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { SMB3_ROM_BYTES = 393232 };
bool smb3_valid_rom(const uint8_t *image, size_t size);
