#include <systemc.h>
#include "icache.h"
#include "main_mem.h"
#include "packets.h"

// =======================================
// 測試專用: PCGenStub (簡單 PC++4)
// 因為 PCGen 還沒寫真的, 這個 stub 暫時只在這個 test 用
// =======================================
SC_MODULE(PCGenStub) {
    sc_in<bool>     clk;
    sc_in<bool>     reset;
    sc_out<uint32_t> out_pc;
    sc_out<bool>    out_valid;
    sc_in<bool>     out_ready;
    SC_CTOR(PCGenStub) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        uint32_t pc = 0;
        out_valid.write(false);
        while (true) {
            wait();
            if (reset.read()) { pc = 0; out_valid.write(false); continue; }
            bool put_out = out_valid.read() && out_ready.read();
            if (put_out) pc = pc + 4;
            out_pc.write(pc);
            out_valid.write(true);
        }
    }
};

SC_MODULE(DecoderStub) {
    sc_in<bool> clk; sc_in<bool> reset;
    sc_in<IfId> in_data; sc_in<bool> in_valid; sc_out<bool> in_ready;
    SC_CTOR(DecoderStub) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        in_ready.write(true);
        while (true) {
            wait();
            if (reset.read()) { in_ready.write(true); continue; }
            in_ready.write(true);
            if (in_valid.read() && in_ready.read()) {
                IfId p = in_data.read();
                std::cout << "[" << sc_time_stamp() << "] Decode: " << p << std::endl;
            }
        }
    }
};

int sc_main(int argc, char* argv[]) {
    g_icache.prefill_inst(0,  0x002082B3);
    g_icache.prefill_inst(4,  0x00500313);
    g_icache.prefill_inst(8,  0x0050B403);
    g_icache.prefill_inst(12, 0x00A0B423);
    g_icache.prefill_inst(16, 0x00208463);
    g_icache.prefill_inst(20, 0x008000EF);

    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> reset_sig;
    sc_signal<bool> redirect_valid_sig;   // tied false (Lesson 11 port)
    sc_signal<uint32_t> pc_sig;
    sc_signal<bool> pc_valid, pc_ready;
    sc_signal<IfId> if_data;
    sc_signal<bool> if_valid, if_ready;

    // ICache <-> MainMem (cache 已預填, 不會 miss, 但 port 還是要綁)
    sc_signal<LineRequest>  mem_req;
    sc_signal<bool>         mem_req_valid, mem_req_ready;
    sc_signal<LineResponse> mem_resp;
    sc_signal<bool>         mem_resp_valid, mem_resp_ready;

    PCGenStub pcgen("pcgen");
    pcgen.clk(clk); pcgen.reset(reset_sig);
    pcgen.out_pc(pc_sig); pcgen.out_valid(pc_valid); pcgen.out_ready(pc_ready);

    ICacheStage icache("icache");
    icache.clk(clk); icache.reset(reset_sig);
    icache.redirect_valid(redirect_valid_sig);
    icache.in_pc(pc_sig); icache.in_valid(pc_valid); icache.in_ready(pc_ready);
    icache.out_data(if_data); icache.out_valid(if_valid); icache.out_ready(if_ready);
    icache.mem_req_data(mem_req); icache.mem_req_valid(mem_req_valid); icache.mem_req_ready(mem_req_ready);
    icache.mem_resp_data(mem_resp); icache.mem_resp_valid(mem_resp_valid); icache.mem_resp_ready(mem_resp_ready);

    MainMem mem("mem");
    mem.clk(clk); mem.reset(reset_sig);
    mem.req_data(mem_req); mem.req_valid(mem_req_valid); mem.req_ready(mem_req_ready);
    mem.resp_data(mem_resp); mem.resp_valid(mem_resp_valid); mem.resp_ready(mem_resp_ready);

    DecoderStub decode("decode");
    decode.clk(clk); decode.reset(reset_sig);
    decode.in_data(if_data); decode.in_valid(if_valid); decode.in_ready(if_ready);

    reset_sig.write(true);  sc_start(10, SC_NS);
    reset_sig.write(false); sc_start(120, SC_NS);
    return 0;
}