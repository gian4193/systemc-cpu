#include "main_mem.h"

// 全域 storage
MainMemStorage g_mainmem;

MainMemStorage::MainMemStorage() { reset(); }

void MainMemStorage::reset() {
    for (int i = 0; i < SIZE; i++) mem[i] = 0;
}

void MainMemStorage::read_line(uint32_t pc, uint32_t* out_line) {
    uint32_t line_start = (pc / 64) * 16;  // 對齊 line 起始 inst index
    for (int i = 0; i < 16; i++) {
        int idx = (int)line_start + i;
        out_line[i] = (idx >= 0 && idx < SIZE) ? mem[idx] : 0;
    }
}

void MainMemStorage::write_inst(uint32_t pc, uint32_t inst) {
    int idx = pc / 4;
    if (idx >= 0 && idx < SIZE) mem[idx] = inst;
}

// =======================================
// MainMem::run() — Multi-cycle stage
// =======================================
void MainMem::run() {
    bool busy = false;
    int  cycles_left = 0;
    LineRequest current_req;
    LineResponse computed_resp;

    req_ready.write(true);
    resp_valid.write(false);

    while (true) {
        wait();
        if (reset.read()) {
            busy = false;
            cycles_left = 0;
            req_ready.write(true);
            resp_valid.write(false);
            continue;
        }

        // === Phase 1: handshake ===
        bool got_req  = req_valid.read()  && req_ready.read();
        bool put_resp = resp_valid.read() && resp_ready.read();

        // === Phase 2: 更新 state ===
        // (a) 上拍 response 被收走 → 釋放
        if (put_resp) {
            busy = false;
            cycles_left = 0;
        }

        // (b) busy 中 + 沒收新 request → 倒數
        if (busy && cycles_left > 0 && !got_req) {
            cycles_left--;
        }

        // (c) 收到新 request → 開始處理
        if (got_req) {
            current_req = req_data.read();
            busy = true;
            cycles_left = icache_cfg::MISS_PENALTY;
            // 馬上把 response 算出來 (邏輯, 不算時序)
            computed_resp.pc = current_req.pc;
            g_mainmem.read_line(current_req.pc, computed_resp.data);
        }

        // === Phase 3: drive output ===
        bool ready_to_send = busy && cycles_left == 0;
        resp_valid.write(ready_to_send);
        if (ready_to_send) resp_data.write(computed_resp);
        req_ready.write(!busy);   // busy 中不能接新 request
    }
}
