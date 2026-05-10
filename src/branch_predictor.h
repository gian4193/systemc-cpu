#pragma once
#include <systemc.h>
#include <cstdint>
#include "packets.h"

// =======================================
// Address slicing helpers
// =======================================
inline uint32_t bp_index(uint32_t pc) { return (pc >> 2) & 0x1F; }   // [6:2]
inline uint32_t bp_tag  (uint32_t pc) { return pc >> 7; }             // [31:7]

// =======================================
// BTB (32 entries, direct-mapped)
// =======================================
struct BTBEntry {
    bool     valid;
    uint32_t tag;
    uint32_t target;
};

class BTB {
public:
    static const uint32_t NUM_ENTRIES = 32;
    BTBEntry entries[NUM_ENTRIES];

    BTB();
    void reset();
    bool lookup(uint32_t pc, uint32_t& out_target);
    void update(uint32_t pc, uint32_t target);
};

// =======================================
// PHT (32 entries, 2-bit counter)
// =======================================
class PHT {
public:
    static const uint32_t NUM_ENTRIES = 32;
    uint8_t counter[NUM_ENTRIES];

    PHT();
    void reset();
    bool predict(uint32_t pc);
    void update(uint32_t pc, bool taken);
};

// =======================================
// Global instances
// =======================================
extern BTB g_btb;
extern PHT g_pht;

// =======================================
// BranchPredictor SC_MODULE
// =======================================
SC_MODULE(BranchPredictor) {
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> redirect_valid;   // ← 新增

    // Lookup interface
    sc_in<uint32_t>   in_pc;
    sc_in<bool>       in_valid;
    sc_out<bool>      in_ready;
    sc_out<BPLookup>  out_data;
    sc_out<bool>      out_valid;
    sc_in<bool>       out_ready;

    // Update interface (no ready, fire-and-forget)
    sc_in<BPUpdate>   upd_data;
    sc_in<bool>       upd_valid;

    SC_CTOR(BranchPredictor) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run();
};
