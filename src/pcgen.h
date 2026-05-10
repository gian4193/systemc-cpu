#pragma once
#include <systemc.h>
#include <cstdint>
#include "packets.h"

// =======================================
// PCGen — PC decision center
// =======================================


//     ┌────────────────┐
//     │     PCGen       │
//     └─┬──────────┬────┘
//       │ pc       │ pc
//       ▼          ▼
// ┌──────────┐ ┌─────────┐
// │ ICache   │ │   BP    │
// └──┬───────┘ └────┬────┘
//     │              │
//     │ IfId         │ BPLookup
//     ▼              ▼
// (Frontend top — Lesson 13 才寫)
// 兩邊都 valid 才 fire


SC_MODULE(PCGen) {
    sc_in<bool> clk;
    sc_in<bool> reset;

    // From backend (redirect)
    sc_in<bool>      redirect_valid;
    sc_in<uint32_t>  redirect_pc;

    // ───────────────────────────────────────────────────────────
    // 兩條 channel, PCGen ↔ BP, 方向相反, 各自獨立 valid/ready:
    //
    //   (A) PC 廣播 channel  PCGen ──► BP
    //       PCGen 寫: out_pc, out_valid
    //       BP    寫: in_ready  (在 PCGen 看叫 bp_ready)
    //       Phase1: both_consumed = icache_ready && bp_ready
    //
    //   (B) Prediction channel  BP ──► PCGen
    //       BP    寫: out_data, out_valid (在 PCGen 看叫 bp_pred_*)
    //       PCGen 寫: bp_pred_ready
    //       Phase1: got_pred = bp_pred_valid && bp_pred_ready
    // ───────────────────────────────────────────────────────────

    // From BranchPredictor (its output) — channel (B)
    sc_in<BPLookup>  bp_pred_data;
    sc_in<bool>      bp_pred_valid;   // BP 說「我有 prediction」
    sc_out<bool>     bp_pred_ready;   // PCGen 說「我能收」(永遠 ready)

    // To ICache + BP (broadcast PC) — channel (A)
    sc_out<uint32_t> out_pc;
    sc_out<bool>     out_valid;
    sc_in<bool>      icache_ready;    // ICache 願不願意收 PC
    sc_in<bool>      bp_ready;        // BP 願不願意收 PC (= BP 的 in_ready)

    SC_CTOR(PCGen) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }
    void run();
};
