#include <systemc.h>
#include "branch_predictor.h"
#include "packets.h"

// =======================================
// PCFeeder: 餵 PC 給 BP, 模擬 PCGen
// =======================================
SC_MODULE(PCFeeder) {
    sc_in<bool>     clk;
    sc_in<bool>     reset;
    sc_out<uint32_t> out_pc;
    sc_out<bool>    out_valid;
    sc_in<bool>     out_ready;

    uint32_t pcs[64];
    int      n_pcs = 0;
    int      idx   = 0;

    SC_CTOR(PCFeeder) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        idx = 0;
        out_valid.write(false);
        while (true) {
            wait();
            if (reset.read()) { idx = 0; out_valid.write(false); continue; }
            bool put_out = out_valid.read() && out_ready.read();
            if (put_out) idx++;
            if (idx < n_pcs) {
                out_pc.write(pcs[idx]);
                out_valid.write(true);
            } else {
                out_valid.write(false);
            }
        }
    }
};

// =======================================
// BPSink: 印出 BP lookup 結果
// =======================================
SC_MODULE(BPSink) {
    sc_in<bool> clk; sc_in<bool> reset;
    sc_in<BPLookup> in_data;
    sc_in<bool>     in_valid;
    sc_out<bool>    in_ready;

    SC_CTOR(BPSink) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        in_ready.write(true);
        while (true) {
            wait();
            if (reset.read()) { in_ready.write(true); continue; }
            in_ready.write(true);
            if (in_valid.read() && in_ready.read()) {
                BPLookup r = in_data.read();
                std::cout << "[" << sc_time_stamp() << "] BP: " << r << std::endl;
            }
        }
    }
};

// =======================================
// UpdateInjector: 在特定 cycle 發 update
// =======================================
SC_MODULE(UpdateInjector) {
    sc_in<bool>     clk;
    sc_in<bool>     reset;
    sc_out<BPUpdate> out_upd;
    sc_out<bool>    out_valid;

    struct Schedule { int cycle; BPUpdate upd; };
    Schedule schedule[16];
    int n_sched = 0;
    int cycle_counter = 0;

    SC_CTOR(UpdateInjector) { SC_THREAD(run); sensitive << clk.pos(); }
    void run() {
        cycle_counter = 0;
        out_valid.write(false);
        while (true) {
            wait();
            if (reset.read()) { cycle_counter = 0; out_valid.write(false); continue; }
            cycle_counter++;
            bool fire = false;
            BPUpdate u;
            for (int i = 0; i < n_sched; i++) {
                if (schedule[i].cycle == cycle_counter) {
                    u = schedule[i].upd;
                    fire = true;
                    std::cout << "[" << sc_time_stamp() << "] UPD: " << u << std::endl;
                    break;
                }
            }
            out_upd.write(u);
            out_valid.write(fire);
        }
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool>     reset_sig;
    sc_signal<bool>     redirect_valid_sig;   // tied false (Lesson 11 port)
    sc_signal<uint32_t> pc_sig;
    sc_signal<bool>     pc_valid, pc_ready;
    sc_signal<BPLookup> lookup_data;
    sc_signal<bool>     lookup_valid, lookup_ready;
    sc_signal<BPUpdate> upd_data;
    sc_signal<bool>     upd_valid;

    PCFeeder feeder("feeder");
    feeder.clk(clk); feeder.reset(reset_sig);
    feeder.out_pc(pc_sig); feeder.out_valid(pc_valid); feeder.out_ready(pc_ready);

    BranchPredictor bp("bp");
    bp.clk(clk); bp.reset(reset_sig);
    bp.redirect_valid(redirect_valid_sig);
    bp.in_pc(pc_sig); bp.in_valid(pc_valid); bp.in_ready(pc_ready);
    bp.out_data(lookup_data); bp.out_valid(lookup_valid); bp.out_ready(lookup_ready);
    bp.upd_data(upd_data); bp.upd_valid(upd_valid);

    BPSink sink("sink");
    sink.clk(clk); sink.reset(reset_sig);
    sink.in_data(lookup_data); sink.in_valid(lookup_valid); sink.in_ready(lookup_ready);

    UpdateInjector injector("injector");
    injector.clk(clk); injector.reset(reset_sig);
    injector.out_upd(upd_data); injector.out_valid(upd_valid);

    // ============================================
    // Test scenario
    //   PC=0x100 是個 branch, target=0x200, 應該 taken
    //   PC=0x300 是個 branch, target=0x400 (not taken 訓練)
    //
    // 預期:
    //   前面幾拍 0x100 lookup → btb_hit=0
    //   update 0x100 後再 lookup 0x100 → btb_hit=1, predict T
    // ============================================
    feeder.pcs[0] = 0x100;
    feeder.pcs[1] = 0x200;
    feeder.pcs[2] = 0x300;
    feeder.pcs[3] = 0x100;
    feeder.pcs[4] = 0x100;
    feeder.pcs[5] = 0x100;
    feeder.pcs[6] = 0x100;
    feeder.pcs[7] = 0x300;
    feeder.pcs[8] = 0x100;
    feeder.n_pcs = 9;

    BPUpdate u1; u1.pc = 0x100; u1.taken = true;  u1.target = 0x200; u1.was_branch = true;
    BPUpdate u2; u2.pc = 0x300; u2.taken = false; u2.target = 0;     u2.was_branch = true;
    BPUpdate u3; u3.pc = 0x100; u3.taken = true;  u3.target = 0x200; u3.was_branch = true;

    injector.schedule[0].cycle = 4; injector.schedule[0].upd = u1;
    injector.schedule[1].cycle = 6; injector.schedule[1].upd = u2;
    injector.schedule[2].cycle = 8; injector.schedule[2].upd = u3;
    injector.n_sched = 3;

    reset_sig.write(true);  sc_start(10, SC_NS);
    reset_sig.write(false); sc_start(150, SC_NS);
    return 0;
}
