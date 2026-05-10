#include <systemc.h>
#include "pcgen.h"
#include "branch_predictor.h"
#include "icache.h"
#include "main_mem.h"
#include "packets.h"

// =======================================
// IfId Sink: 印出 frontend 拿到的 inst
// =======================================
SC_MODULE(IfIdSink) {
    sc_in<bool> clk; sc_in<bool> reset;
    sc_in<bool> redirect_valid;   // ★ 下游也得認 redirect, 不然會有 1-cycle leak
    sc_in<IfId>  in_data;
    sc_in<bool>  in_valid;
    sc_out<bool> in_ready;

    SC_CTOR(IfIdSink) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        in_ready.write(true);
        while (true) {
            wait();
            if (reset.read()) { in_ready.write(true); continue; }
            in_ready.write(true);
            if (in_valid.read() && in_ready.read() && !redirect_valid.read()) {
                IfId p = in_data.read();
                std::cout << "[" << sc_time_stamp() << "] FETCH: " << p << std::endl;
            }
        }
    }
};

// =======================================
// BPSink: 印出 BP prediction (debug 用)
// =======================================
SC_MODULE(BPSink) {
    sc_in<bool> clk; sc_in<bool> reset;
    sc_in<BPLookup> in_data;
    sc_in<bool>     in_valid;

    SC_CTOR(BPSink) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        while (true) {
            wait();
            if (reset.read()) continue;
            if (in_valid.read()) {
                BPLookup p = in_data.read();
                if (p.btb_hit) {
                    std::cout << "[" << sc_time_stamp() << "]   BP: " << p << std::endl;
                }
            }
        }
    }
};

// =======================================
// RedirectInjector: 在特定 cycle 發 redirect
// =======================================
SC_MODULE(RedirectInjector) {
    sc_in<bool>      clk;
    sc_in<bool>      reset;
    sc_out<bool>     redirect_valid;
    sc_out<uint32_t> redirect_pc;

    int cycle_counter = 0;

    struct Schedule { int cycle; uint32_t pc; };
    Schedule schedule[8];
    int n_sched = 0;

    SC_CTOR(RedirectInjector) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        cycle_counter = 0;
        redirect_valid.write(false);
        while (true) {
            wait();
            if (reset.read()) {
                cycle_counter = 0;
                redirect_valid.write(false);
                continue;
            }
            cycle_counter++;
            bool fire = false;
            uint32_t target = 0;
            for (int i = 0; i < n_sched; i++) {
                if (schedule[i].cycle == cycle_counter) {
                    target = schedule[i].pc;
                    fire = true;
                    std::cout << "[" << sc_time_stamp() << "] === REDIRECT to pc=" << target << " ===" << std::endl;
                    break;
                }
            }
            redirect_valid.write(fire);
            if (fire) redirect_pc.write(target);
        }
    }
};

int sc_main(int argc, char* argv[]) {
    // Setup MainMem with some test program
    g_mainmem.write_inst(0,  0x002082B3);
    g_mainmem.write_inst(4,  0x00500313);
    g_mainmem.write_inst(8,  0x0050B403);
    g_mainmem.write_inst(12, 0x00A0B423);
    g_mainmem.write_inst(16, 0x00208463);
    g_mainmem.write_inst(20, 0x008000EF);

    g_mainmem.write_inst(0x200, 0xDEADBEEF);
    g_mainmem.write_inst(0x204, 0xCAFEBABE);
    g_mainmem.write_inst(0x208, 0x12345678);

    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> reset_sig;

    // Redirect signals
    sc_signal<bool>      redirect_valid;
    sc_signal<uint32_t>  redirect_pc;

    // PCGen → ICache + BP
    sc_signal<uint32_t>  pc_sig;
    sc_signal<bool>      pc_valid;
    sc_signal<bool>      icache_ready, bp_ready;

    // BP → PCGen + Sink
    sc_signal<BPLookup>  bp_data;
    sc_signal<bool>      bp_valid, bp_ready_back;

    // BP update (這次 testbench 不主動更新)
    sc_signal<BPUpdate>  upd_data;
    sc_signal<bool>      upd_valid;

    // ICache → IfId Sink
    sc_signal<IfId>      ifid_data;
    sc_signal<bool>      ifid_valid, ifid_ready;

    // ICache ↔ MainMem
    sc_signal<LineRequest>  mem_req;
    sc_signal<bool>         mem_req_valid, mem_req_ready;
    sc_signal<LineResponse> mem_resp;
    sc_signal<bool>         mem_resp_valid, mem_resp_ready;

    // ===== Modules =====
    PCGen pcgen("pcgen");
    pcgen.clk(clk); pcgen.reset(reset_sig);
    pcgen.redirect_valid(redirect_valid); pcgen.redirect_pc(redirect_pc);
    pcgen.bp_pred_data(bp_data); pcgen.bp_pred_valid(bp_valid); pcgen.bp_pred_ready(bp_ready_back);
    pcgen.out_pc(pc_sig); pcgen.out_valid(pc_valid);
    pcgen.icache_ready(icache_ready); pcgen.bp_ready(bp_ready);

    ICacheStage icache("icache");
    icache.clk(clk); icache.reset(reset_sig);
    icache.redirect_valid(redirect_valid);
    icache.in_pc(pc_sig); icache.in_valid(pc_valid); icache.in_ready(icache_ready);
    icache.out_data(ifid_data); icache.out_valid(ifid_valid); icache.out_ready(ifid_ready);
    icache.mem_req_data(mem_req); icache.mem_req_valid(mem_req_valid); icache.mem_req_ready(mem_req_ready);
    icache.mem_resp_data(mem_resp); icache.mem_resp_valid(mem_resp_valid); icache.mem_resp_ready(mem_resp_ready);

    BranchPredictor bp("bp");
    bp.clk(clk); bp.reset(reset_sig);
    bp.redirect_valid(redirect_valid);
    bp.in_pc(pc_sig); bp.in_valid(pc_valid); bp.in_ready(bp_ready);
    bp.out_data(bp_data); bp.out_valid(bp_valid); bp.out_ready(bp_ready_back);
    bp.upd_data(upd_data); bp.upd_valid(upd_valid);

    MainMem mem("mem");
    mem.clk(clk); mem.reset(reset_sig);
    mem.req_data(mem_req); mem.req_valid(mem_req_valid); mem.req_ready(mem_req_ready);
    mem.resp_data(mem_resp); mem.resp_valid(mem_resp_valid); mem.resp_ready(mem_resp_ready);

    IfIdSink fetch_sink("fetch_sink");
    fetch_sink.clk(clk); fetch_sink.reset(reset_sig);
    fetch_sink.redirect_valid(redirect_valid);
    fetch_sink.in_data(ifid_data); fetch_sink.in_valid(ifid_valid); fetch_sink.in_ready(ifid_ready);

    BPSink bp_sink("bp_sink");
    bp_sink.clk(clk); bp_sink.reset(reset_sig);
    bp_sink.in_data(bp_data); bp_sink.in_valid(bp_valid);

    RedirectInjector injector("injector");
    injector.clk(clk); injector.reset(reset_sig);
    injector.redirect_valid(redirect_valid); injector.redirect_pc(redirect_pc);

    // ===== Test scenario =====
    // Cycle 20: 跑了幾條指令後, 假裝 backend 算出「真實要跳到 0x200」
    injector.schedule[0].cycle = 20;
    injector.schedule[0].pc = 0x200;
    injector.n_sched = 1;

    reset_sig.write(true);  sc_start(10, SC_NS);
    reset_sig.write(false); sc_start(400, SC_NS);
    return 0;
}
