#include "icache.h"

// 全域 ICache instance 的 storage
ICache g_icache;

// =======================================
// ICache class methods
// =======================================
ICache::ICache() { reset(); }

void ICache::reset() {
    for (uint32_t s = 0; s < icache_cfg::NUM_SETS; s++)
        for (uint32_t w = 0; w < icache_cfg::NUM_WAYS; w++)
            lines[s][w].valid = false;
}

bool ICache::lookup(uint32_t pc, uint32_t& out_inst) {
    uint32_t idx = icache_index(pc);
    uint32_t tag = icache_tag(pc);
    uint32_t inst_idx = icache_inst_idx_in_line(pc);
    for (uint32_t w = 0; w < icache_cfg::NUM_WAYS; w++) {
        CacheLine& line = lines[idx][w];
        if (line.valid && line.tag == tag) {
            out_inst = line.data[inst_idx];
            return true;
        }
    }
    return false;
}

void ICache::prefill_inst(uint32_t pc, uint32_t inst) {
    uint32_t idx = icache_index(pc);
    uint32_t tag = icache_tag(pc);
    uint32_t inst_idx = icache_inst_idx_in_line(pc);
    for (uint32_t w = 0; w < icache_cfg::NUM_WAYS; w++) {
        CacheLine& line = lines[idx][w];
        if (line.valid && line.tag == tag) {
            line.data[inst_idx] = inst;
            return;
        }
    }
    for (uint32_t w = 0; w < icache_cfg::NUM_WAYS; w++) {
        CacheLine& line = lines[idx][w];
        if (!line.valid) {
            line.valid = true;
            line.tag = tag;
            for (uint32_t i = 0; i < icache_cfg::INSTS_PER_LINE; i++) line.data[i] = 0;
            line.data[inst_idx] = inst;
            return;
        }
    }
}

// =======================================
// ICacheStage::run()  <-- 你寫 TODO 那段
// =======================================
void ICacheStage::run() {
    bool full = false;
    IfId stored;
    uint32_t pc ;

    in_ready.write(true);
    out_valid.write(false);

    while (true) {
        wait();
        if (reset.read()) {
            full = false;
            in_ready.write(true);
            out_valid.write(false);
            continue;
        }

        // ============================================
        //  TODO: 三段式 + cache lookup
        //
        // Phase 1: handshake
        //   bool got_in  = ...
        //   bool put_out = ...
        //
        // Phase 2:
        //   if (put_out) full = false;
        //   if (got_in) {
        //       uint32_t pc = in_pc.read();
        //       uint32_t inst;
        //       g_icache.lookup(pc, inst);  // 假設 hit
        //       IfId p; p.pc = pc; p.inst = inst;
        //       stored = p;
        //       full = true;
        //   }
        //
        // Phase 3:
        //   out_valid.write(full);
        //   if (full) out_data.write(stored);
        //   in_ready.write(!full);
        // ============================================
        bool got_in  = in_valid.read() && in_ready.read();
        bool put_out = out_valid.read() && out_ready.read();

        if(put_out) full = false;
        if(got_in){
            full = true;
            pc = in_pc.read();
        }
        bool lookup_cache = false;
        if(full){
            uint32_t inst;
            lookup_cache= g_icache.lookup(pc, inst);
            if(lookup_cache){
                IfId p; p.pc = pc; p.inst = inst;
                stored=p;
            }
        }
        

        out_valid.write(lookup_cache);
        if(lookup_cache) out_data.write(stored);
        in_ready.write(!full);
    }
}
