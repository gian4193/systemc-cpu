#include "branch_predictor.h"

// Global storage
BTB g_btb;
PHT g_pht;

// =======================================
// BTB methods
// =======================================
BTB::BTB() { reset(); }

void BTB::reset() {
    for (uint32_t i = 0; i < NUM_ENTRIES; i++) {
        entries[i].valid = false;
        entries[i].tag = 0;
        entries[i].target = 0;
    }
}

bool BTB::lookup(uint32_t pc, uint32_t& out_target) {
    uint32_t idx = bp_index(pc);
    uint32_t tag = bp_tag(pc);
    BTBEntry& e = entries[idx];
    if (e.valid && e.tag == tag) {
        out_target = e.target;
        return true;
    }
    return false;
}

void BTB::update(uint32_t pc, uint32_t target) {
    uint32_t idx = bp_index(pc);
    uint32_t tag = bp_tag(pc);
    BTBEntry& e = entries[idx];
    e.valid = true;
    e.tag = tag;
    e.target = target;
}

// =======================================
// PHT methods
// =======================================
PHT::PHT() { reset(); }

void PHT::reset() {
    // 初始化成 WEAK_NT (01) — 中間偏向 not-taken
    for (uint32_t i = 0; i < NUM_ENTRIES; i++) {
        counter[i] = 0b01;
    }
}

bool PHT::predict(uint32_t pc) {
    uint32_t idx = bp_index(pc);
    return (counter[idx] >> 1) != 0;   // MSB
}

void PHT::update(uint32_t pc, bool taken) {
    uint32_t idx = bp_index(pc);
    uint8_t& c = counter[idx];
    if (taken) {
        if (c < 0b11) c++;
    } else {
        if (c > 0b00) c--;
    }
}

// =======================================
// BranchPredictor::run()
// =======================================
void BranchPredictor::run() {
    bool full = false;
    BPLookup stored;

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

        // ★ NEW: redirect 處理
        // ============================================
        // TODO:
        // if (redirect_valid.read()) {
        //     full = false;            // 丟掉 stored prediction
        //     in_ready.write(true);
        //     out_valid.write(false);
        //     continue;
        // }
        //
        // 注意: BTB / PHT 內部資料 不要清!
        //   它們是「學到的」狀態, redirect 不該影響
        // ============================================

        // === Phase 1: handshake ===
        bool got_in  = in_valid.read()  && in_ready.read();
        bool put_out = out_valid.read() && out_ready.read();
        bool got_upd = upd_valid.read();   // ★ 沒 ready, 看到 valid 就吃

        // === Phase 2: update path (任何 cycle 都可以) ===
        if (got_upd) {
            BPUpdate u = upd_data.read();
            g_pht.update(u.pc, u.taken);
            if (u.was_branch && u.taken) {
                g_btb.update(u.pc, u.target);
            }
        }

        // === Phase 2: lookup path ===
        if (put_out) full = false;
        if (got_in) {
            uint32_t pc = in_pc.read();
            BPLookup r;
            r.pc = pc;
            r.btb_hit = g_btb.lookup(pc, r.target);
            r.predict_taken = g_pht.predict(pc);
            stored = r;
            full = true;
        }

        // === Phase 3: drive output ===
        out_valid.write(full);
        if (full) out_data.write(stored);
        in_ready.write(!full);
    }
}
