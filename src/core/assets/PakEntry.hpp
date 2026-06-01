#pragma once
#include <cstdint>

struct PakEntry {
    uint64_t uuid;
    uint64_t offset;
    uint64_t size;
    char path[256];
};