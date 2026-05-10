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

// ★ 新方法: fill 整個 line
void ICache::fill_line(uint32_t pc, const uint32_t* line_data) {
    uint32_t idx = icache_index(pc);
    uint32_t tag = icache_tag(pc);
    // 簡單版: 找第一個 invalid way, 沒有就 way 0 (no LRU yet)
    int target_way = 0;
    for (uint32_t w = 0; w < icache_cfg::NUM_WAYS; w++) {
        if (!lines[idx][w].valid) { target_way = w; break; }
    }
    CacheLine& line = lines[idx][target_way];
    line.valid = true;
    line.tag = tag;
    for (uint32_t i = 0; i < icache_cfg::INSTS_PER_LINE; i++) {
        line.data[i] = line_data[i];
    }
}

// =======================================
// ICacheStage::run() — 兩個 state: IDLE / MISS_PENDING
// =======================================
void ICacheStage::run() {
    enum State { IDLE, MISS_PENDING };
    State state = IDLE;

    bool full = false;          // out 端: 我手上有 IfId 待送嗎
    IfId stored;                // out 端: 待送的 IfId
    uint32_t miss_pc = 0;       // miss 中: 卡在哪個 PC
    bool req_sent = false;      // request 已經被 MainMem 收下了嗎

    in_ready.write(true);
    out_valid.write(false);
    mem_req_valid.write(false);
    mem_resp_ready.write(false);

    while (true) {
        wait();
        if (reset.read()) {
            state = IDLE;
            full = false;
            req_sent = false;
            in_ready.write(true);
            out_valid.write(false);
            mem_req_valid.write(false);
            mem_resp_ready.write(false);
            continue;
        }

        // === Phase 1: 觀察所有 handshake ===
        bool got_in   = in_valid.read()       && in_ready.read();
        bool put_out  = out_valid.read()      && out_ready.read();
        bool sent_req = mem_req_valid.read()  && mem_req_ready.read();
        bool got_resp = mem_resp_valid.read() && mem_resp_ready.read();

        // === Phase 2: 更新 state ===
        if (put_out) full = false;

        if (state == IDLE) {
            if (got_in) {
                uint32_t pc = in_pc.read();
                uint32_t inst;
                bool hit = g_icache.lookup(pc, inst);
                if (hit) {
                    IfId p; p.pc = pc; p.inst = inst;
                    stored = p;
                    full = true;
                } else {
                    miss_pc = pc;
                    req_sent = false;
                    state = MISS_PENDING;
                }
            }
        } else { // MISS_PENDING
            if (sent_req) req_sent = true;
            if (got_resp) {
                LineResponse r = mem_resp_data.read();
                g_icache.fill_line(r.pc, r.data);
                uint32_t inst;
                g_icache.lookup(miss_pc, inst);  // 一定 hit
                IfId p; p.pc = miss_pc; p.inst = inst;
                stored = p;
                full = true;
                state = IDLE;
                req_sent = false;
            }
        }

        // === Phase 3: drive output ===
        out_valid.write(full);
        if (full) out_data.write(stored);
        in_ready.write(state == IDLE && !full);

        // 對 MainMem
        bool need_req = (state == MISS_PENDING) && !req_sent;
        mem_req_valid.write(need_req);
        if (need_req) {
            LineRequest r;
            r.pc = line_aligned_pc(miss_pc);
            mem_req_data.write(r);
        }
        mem_resp_ready.write(state == MISS_PENDING);
    }
}
