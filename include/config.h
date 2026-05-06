#pragma once

// TODO: tunable parameters (ICACHE_NUM_SETS, BTB_ENTRIES, ...)
#include <cstdint>

// ICache 規格
namespace icache_cfg {
    static const uint32_t LINE_BYTES      = 64;
    static const uint32_t NUM_SETS        = 16;
    static const uint32_t NUM_WAYS        = 4;
    static const uint32_t INSTS_PER_LINE  = 16;
    static const uint32_t MISS_PENALTY    = 10;  // Lesson 9 用
}

// BTB 規格 (Lesson 10 用)
namespace btb_cfg {
    static const uint32_t NUM_ENTRIES     = 32;
}

// FetchQueue (Lesson 12 用)
namespace fq_cfg {
    static const uint32_t DEPTH           = 8;
}