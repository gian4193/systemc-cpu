#include "pcgen.h"

void PCGen::run() {
    uint32_t pc = 0;

    out_valid.write(false);
    bp_pred_ready.write(true);   // 永遠 ready 收 BP prediction

    while (true) {
        wait();
        if (reset.read()) {
            pc = 0;
            out_valid.write(false);
            bp_pred_ready.write(true);
            continue;
        }

        bp_pred_ready.write(false);
        out_valid.write(false);

        // out_valid.read() 讀「上一拍我寫了什麼」, 必須加進來:
        //   reset 完第一拍 out_valid 還是 0 (init/reset 寫的),
        //   沒它就會誤判成 "都 ready 了→ 上一拍 pc 已被收"
        //   結果 PCGen 在 cycle 1 直接 pc 0→4, pc=0 永遠 fetch 不到
        bool both_consumed = out_valid.read() && icache_ready.read() && bp_ready.read();
        bool got_pred      = bp_pred_valid.read() && bp_pred_ready.read();
        // ──────────────────────────────────────────────────────────────
        // ★ stall 期間 got_pred 會 toggle 0/1
        //
        // ICache miss / 任何下游 stall 時 both_consumed=0, PCGen 卡在原 pc。
        // 但 out_valid 永遠 1, BP 每次自己 buffer 空了又被 PCGen 餵同一個 pc,
        // 1-cycle latency 後再吐同一個 prediction → got_pred 在 0/1 之間 toggle。
        //
        // 為什麼 OK:
        //   PCGen 只在 both_consumed=1 那拍才用 got_pred 算 next pc。
        //   stall 期間 both_consumed=0, got_pred 是啥都不會被讀進 if 分支。
        //   重新推進時當下 got_pred=1 就用 BP target, =0 就 fallback pc+4
        //   (兩個結果都是合法的, BP miss-predict 之後 backend 會 redirect 修)。
        //
        // 唯一代價: BP 被同一個 pc 重算很多次, 浪費能量但不影響正確性。
        // 想省可以 stall 時把 out_valid 拉低, 但 spec 要求 "永遠 valid"。
        // ──────────────────────────────────────────────────────────────

        if(redirect_valid.read()){
            pc = redirect_pc.read();
        }
        else if(both_consumed){
            if(got_pred){
                BPLookup bp = bp_pred_data.read();
                if(bp.btb_hit && bp.predict_taken){
                    pc = bp.target;
                }
                else{
                    pc = pc + 4;   // ★ btb miss / not-taken → fallthrough
                }
            }
            else{
                pc = pc + 4;
            }
        }

        bp_pred_ready.write(true);
        out_valid.write(true);
        out_pc.write(pc);


        // ============================================
        // TODO: Phase 1, 2, 3
        //
        // Phase 1 — 觀察:
        //   bool both_consumed = icache_ready.read() && bp_ready.read();
        //   bool got_pred      = bp_pred_valid.read() && bp_pred_ready.read();
        //
        // Phase 2 — 算 next pc (priority 邏輯):
        //
        //   if (redirect_valid.read()) {
        //       pc = redirect_pc.read();    // ← 最高優先, 直接覆蓋
        //   }
        //   else if (both_consumed) {
        //       // PC 被收下, 算下一拍
        //       if (got_pred) {
        //           BPLookup p = bp_pred_data.read();
        //           if (p.btb_hit && p.predict_taken) {
        //               pc = p.target;
        //           } else {
        //               pc = pc + 4;
        //           }
        //       } else {
        //           // BP 還沒給 prediction (例如 reset 剛結束第一拍)
        //           pc = pc + 4;
        //       }
        //   }
        //   // else: pc 不動 (stall, 等下游 ready)
        //
        // Phase 3 — drive output:
        //   out_pc.write(pc);
        //   out_valid.write(true);    // ★ 永遠 valid
        //   bp_pred_ready.write(true);
        // ============================================

    }
}
