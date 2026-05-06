#pragma once
#include <systemc.h>
#include <cstdint>
#include "packets.h"
#include "config.h"

// =======================================
// ICache 是普通 C++ class (storage element)
// =======================================
struct CacheLine {
    bool     valid;
    uint32_t tag;
    uint32_t data[icache_cfg::INSTS_PER_LINE];
};

class ICache {
public:
    CacheLine lines[icache_cfg::NUM_SETS][icache_cfg::NUM_WAYS];
    
    ICache();
    void reset();
    bool lookup(uint32_t pc, uint32_t& out_inst);
    void prefill_inst(uint32_t pc, uint32_t inst);
};

// 全域 instance (.cpp 才定義 storage)
extern ICache g_icache;

// Address 切片 helper
inline uint32_t icache_offset(uint32_t pc) { return pc & 0x3F; }
inline uint32_t icache_index (uint32_t pc) { return (pc >> 6) & 0xF; }
inline uint32_t icache_tag   (uint32_t pc) { return pc >> 10; }
inline uint32_t icache_inst_idx_in_line(uint32_t pc) { return (pc & 0x3F) / 4; }

// =======================================
// ICacheStage 是 SystemC module (timed wrapper)
// =======================================
SC_MODULE(ICacheStage) {
    sc_in<bool>     clk;
    sc_in<bool>     reset;
    sc_in<uint32_t> in_pc;
    sc_in<bool>     in_valid;
    sc_out<bool>    in_ready;
    sc_out<IfId>    out_data;
    sc_out<bool>    out_valid;
    sc_in<bool>     out_ready;

    SC_CTOR(ICacheStage) { SC_THREAD(run); sensitive << clk.pos(); }
    void run();   // 實作在 .cpp
};