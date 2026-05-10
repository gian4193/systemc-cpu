#pragma once
#include <systemc.h>
#include <cstdint>
#include "packets.h"
#include "config.h"

// =======================================
// MainMemStorage: 背後的真 memory (storage class, 不是 SC_MODULE)
// =======================================
class MainMemStorage {
public:
    static const int SIZE = 1024;  // 1024 inst = 4 KB
    uint32_t mem[SIZE];
    MainMemStorage();
    void reset();
    void read_line(uint32_t pc, uint32_t* out_line);
    void write_inst(uint32_t pc, uint32_t inst);
};
extern MainMemStorage g_mainmem;

// =======================================
// MainMem: SC_MODULE, multi-cycle latency
// 收 LineRequest → 等 N cycle → 送 LineResponse
// =======================================
SC_MODULE(MainMem) {
    sc_in<bool> clk;
    sc_in<bool> reset;

    // request 進來 (從 ICacheStage)
    sc_in<LineRequest> req_data;
    sc_in<bool>        req_valid;
    sc_out<bool>       req_ready;

    // response 出去 (給 ICacheStage)
    sc_out<LineResponse> resp_data;
    sc_out<bool>         resp_valid;
    sc_in<bool>          resp_ready;

    SC_CTOR(MainMem) { SC_THREAD(run); sensitive << clk.pos(); }
    void run();
};
