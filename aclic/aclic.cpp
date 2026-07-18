/* aclic.cpp — DUT 模板。妳的戰場是五個 TODO;每個標了 SPEC 規則編號。 */
#include "aclic.hpp"
#include <cassert>

void Aclic::reset() {                               /* R7,已實作 */
    sourcecfg_.fill(Sm::Inactive);
    iprio_.fill(0xFF);
    setip_.fill(false);
    setie_.fill(false);
    line_.fill(false);
    ithreshold_ = 0;
}

/* ------------------------------------------------------------------ */
/* TODO(1, R1+R2 共用核心):level 類的「鏡子」重估 helper。
 * 給定 src,依 sourcecfg_ 與 line_ 現值判斷「條件是否成立」並維護 setip_。
 * set_line / sw_setip / claim 三個入口都會用到它——
 * 想清楚它就等於想清楚「pending 的三個寫入者」怎麼共存。
 * (在 aclic.hpp 的 private 區補上宣告) */

void Aclic::set_sourcecfg(int src, Sm sm) {
    sourcecfg_[src] = sm;
        if(sm==Sm::Inactive) setip_[src] = 0;
    /* TODO(1a, R1):切到 Inactive 時 pending 該怎麼辦?「恆 0」是線索。 */
}

void Aclic::set_line(int src, bool level) {
    /* TODO(2, R1):
     *  - Edge1/Edge0:偵測「跳變」——需要 line_ 的舊值。
     *  - Level1/Level0:鏡子語意。
     *  - Inactive/Detached:電位變化無效(但 line_ 要不要照記?
     *    想想:Detached 之後切回 Level1,鏡子照的是什麼?)
     *  最後記得更新 line_[src]。 */
    if(sourcecfg_[src] == Sm::Level1){
        if(level) setip_[src]=1; 
        else setip_[src] = 0;
    }
    else if(sourcecfg_[src] == Sm::Level0){
        if(!level) setip_[src]=1; 
        else setip_[src] = 0;
    }
    else if(sourcecfg_[src] == Sm::Edge1){
        bool current_line = line_[src];
        if(!current_line && level) {
            setip_[src] = 1;
        }else{
            setip_[src] = 0;
        }
    }
    else if(sourcecfg_[src] == Sm::Edge0){
        bool current_line = line_[src];
        if(current_line && !level) {
            setip_[src] = 1;
        }else{
            setip_[src] = 0;
        }
    }
    line_[src] = level;
    
}

void Aclic::sw_setip(int src, bool pend) {
    /* TODO(3, R2):Inactive 無效;Level 類 clear 後鏡子若仍成立立即重建;其餘照寫。 */
    if(sourcecfg_[src]==Sm::Inactive) return;
    setip_[src] = pend;

    if(sourcecfg_[src]==Sm::Level1){
        if(line_[src]) setip_[src] = 1;
        else setip_[src] = 0;
    }
    else if(sourcecfg_[src]==Sm::Level0){
        if(!line_[src]) setip_[src] = 1;
        else setip_[src] = 0;
    }
}

uint32_t Aclic::topi() const {
    /* TODO(4, R3+R4):
     *  - 參賽資格:setip ∧ setie ∧ 過門檻(注意不等式方向與 P=0 特例)。
     *  - 勝者:iprio 最小,同 iprio 取 id 較小。
     *  - 回傳 (id<<16)|iprio;無人參賽回 0。
     *  - const:改 state 過不了編譯,這是 R3「純函數」的型別版。 */
    int winner = 0;
    for(int i=0; i<NSRC; i++){
        if(setie_[i] && setip_[i]){
            if(ithreshold_==0 || iprio_[i] < ithreshold_){
                if(winner==0 || iprio_[i]<iprio_[winner]){
                    winner = i;
                }
            }
        }
    }
    if(!winner) return 0;
    assert(winner > 0 && winner < NSRC);
    return ((winner<<16)|iprio_[winner]);
}

uint32_t Aclic::claim() {
    /* TODO(5, R5):
     *  - 取當下 topi();為 0 直接回 0(spurious,什麼都不清)。
     *  - 清贏家 setip_ —— 然後 "if possible":Level 且條件仍成立呢?
     *    (TODO(1) 的 helper 在這裡回收) */
    uint32_t t = topi();
    if(t == 0) return 0;               /* spurious:什麼都不清 */

    int winner = (t >> 16);
    assert(winner > 0 && winner < NSRC);
    setip_[winner] = 0;

    /* "if possible":Level 且線仍拉著 → 鏡子立即重建(R5) */
    if(sourcecfg_[winner]==Sm::Level1 && line_[winner]) setip_[winner] = 1;
    else if(sourcecfg_[winner]==Sm::Level0 && !line_[winner]) setip_[winner] = 1;

    return t;
}
