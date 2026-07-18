/* test_aclic.cpp — testbench(勿改)。T4/T5 對應舊 Q5,T3/T10 對應舊 Q6。 */
#include <cstdio>
#include "aclic.hpp"

static int fails = 0, checks = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; std::printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)
static constexpr uint32_t TOPI(int id, int prio) { return (uint32_t(id) << 16) | uint32_t(prio); }
using Sm = Aclic::Sm;

static void t1_edge_basic() {
    std::printf("T1  edge 基本生命週期 (R1/R5/R6)\n");
    Aclic a;
    a.set_sourcecfg(5, Sm::Edge1);
    a.set_iprio(5, 10);
    a.set_ie(5, true);
    CHECK(!a.irq_out(), "還沒拉線就不該有中斷");
    a.set_line(5, true);
    CHECK(a.peek_ip(5), "上升緣應置 pending");
    CHECK(a.topi() == TOPI(5, 10), "topi 應為 (5,10)");
    CHECK(a.claim() == TOPI(5, 10), "claim 回傳贏家");
    CHECK(!a.peek_ip(5), "claim 後 pending 應清除");
    CHECK(a.topi() == 0, "無贏家 topi=0");
    a.set_line(5, true);
    CHECK(!a.peek_ip(5), "line 維持高:edge 不重觸發");
    a.set_line(5, false);
    CHECK(!a.peek_ip(5), "Edge1 對下降緣無感");
}

static void t2_arbitration() {
    std::printf("T2  仲裁與 tiebreak (R3)\n");
    Aclic a;
    for (int s : {3, 7}) { a.set_sourcecfg(s, Sm::Edge1); a.set_ie(s, true); }
    a.set_iprio(3, 20);
    a.set_iprio(7, 10);
    a.set_line(3, true);
    a.set_line(7, true);
    CHECK(a.topi() == TOPI(7, 10), "iprio 小者勝(7 勝 3)");
    a.set_iprio(3, 10);
    CHECK(a.topi() == TOPI(3, 10), "同 iprio 取 id 小(3 勝 7)");
}

static void t3_threshold_boundary() {
    std::printf("T3  門檻邊界:>= 擋、< 放 (R4)\n");
    Aclic a;
    a.set_sourcecfg(6, Sm::Edge1);
    a.set_ie(6, true);
    a.set_iprio(6, 5);
    a.set_line(6, true);
    a.set_threshold(5);
    CHECK(a.topi() == 0, "iprio 5、thr 5:5>=5 應被擋(妳的舊 Q6)");
    a.set_threshold(6);
    CHECK(a.topi() == TOPI(6, 5), "iprio 5、thr 6:5<6 應放行");
    a.set_threshold(0);
    CHECK(a.topi() == TOPI(6, 5), "thr 0 = 不設限");
}

static void t4_level_mirror() {
    std::printf("T4  level = 鏡子:claim 重建、降線自清 (R1/R5,妳的舊 Q5)\n");
    Aclic a;
    a.set_sourcecfg(2, Sm::Level1);
    a.set_ie(2, true);
    a.set_iprio(2, 3);
    a.set_line(2, true);
    CHECK(a.peek_ip(2), "line 高:pending 成立");
    CHECK(a.claim() == TOPI(2, 3), "claim 拿到贏家");
    CHECK(a.peek_ip(2), "line 仍高:pending 應立即重建(interrupt storm 的根)");
    CHECK(a.irq_out(), "所以 irq_out 還亮著");
    a.set_line(2, false);
    CHECK(!a.peek_ip(2), "條件消失:鏡子自清,不需 claim");
}

static void t5_edge_silent_loss() {
    std::printf("T5  edge 的靜默死法:claim 後線仍高,不會再有上升緣 (R1)\n");
    Aclic a;
    a.set_sourcecfg(4, Sm::Edge1);
    a.set_ie(4, true);
    a.set_iprio(4, 8);
    a.set_line(4, true);
    (void)a.claim();
    CHECK(!a.peek_ip(4), "claim 清了 pending");
    a.set_line(4, true);
    CHECK(!a.peek_ip(4), "沒有跳變 → 中斷從此消失(靜默掉資料)");
    a.set_line(4, false);
    a.set_line(4, true);
    CHECK(a.peek_ip(4), "重新有跳變才復活");
}

static void t6_winner_vanishes() {
    std::printf("T6  贏家蒸發與 spurious (R3/R5,妳的舊 Q8)\n");
    Aclic a;
    for (int s : {3, 9}) { a.set_sourcecfg(s, Sm::Edge1); a.set_ie(s, true); a.set_line(s, true); }
    a.set_iprio(3, 10);
    a.set_iprio(9, 20);
    a.set_ie(3, false);
    CHECK(a.claim() == TOPI(9, 20), "claim 應拿到重算後的新贏家 9");
    CHECK(a.peek_ip(3), "src3 的 pending 不受影響(它只是失去參賽資格)");
    a.set_line(9, false);
    a.set_line(9, true);
    a.set_ie(9, false);
    CHECK(a.claim() == 0, "無人參賽:spurious 回 0");
    CHECK(a.peek_ip(3) && a.peek_ip(9), "spurious 不得清任何 pending");
}

static void t7_claim_clears_only_winner() {
    std::printf("T7  claim 只清贏家一人 (R5)\n");
    Aclic a;
    for (int s : {1, 2}) { a.set_sourcecfg(s, Sm::Edge1); a.set_ie(s, true); a.set_line(s, true); }
    a.set_iprio(1, 1);
    a.set_iprio(2, 2);
    CHECK(a.claim() == TOPI(1, 1), "先拿最急的");
    CHECK(!a.peek_ip(1) && a.peek_ip(2), "只清 1,2 還在排隊");
    CHECK(a.claim() == TOPI(2, 2), "第二次 claim 輪到 2");
}

static void t8_detached() {
    std::printf("T8  detached:線斷開、軟體可注入 (R1/R2)\n");
    Aclic a;
    a.set_sourcecfg(11, Sm::Detached);
    a.set_ie(11, true);
    a.set_iprio(11, 7);
    a.set_line(11, true);
    CHECK(!a.peek_ip(11), "線已斷開:電位無效");
    a.sw_setip(11, true);
    CHECK(a.topi() == TOPI(11, 7), "軟體注入生效");
    CHECK(a.claim() == TOPI(11, 7), "claim 正常");
    CHECK(!a.peek_ip(11), "detached 無鏡子:清了就是清了");
}

static void t9_inactive() {
    std::printf("T9  inactive:這條線不存在 (R1/R2)\n");
    Aclic a;
    a.set_ie(13, true);
    a.set_iprio(13, 1);
    a.set_line(13, true);
    a.sw_setip(13, true);
    CHECK(!a.peek_ip(13), "線與軟體 set 皆無效,setip 恆 0");
    CHECK(a.topi() == 0, "自然也不參賽");
}

static void t10_threshold_masks_arb_not_storage() {
    std::printf("T10 門檻濾參賽資格,不濾儲存 (R4)\n");
    Aclic a;
    a.set_sourcecfg(8, Sm::Edge1);
    a.set_ie(8, true);
    a.set_iprio(8, 9);
    a.set_line(8, true);
    a.set_threshold(5);
    CHECK(a.topi() == 0, "被門檻擋:不參賽");
    CHECK(a.peek_ip(8), "但 pending 儲存還在——擋的是仲裁,不是記憶");
    a.set_threshold(0);
    CHECK(a.topi() == TOPI(8, 9), "門檻一放,原地復活(tail 效果的來源)");
}

static void t11_level_sw_clear_futile() {
    std::printf("T11 level 鏡子不接受欺騙:軟體 clear 後立即重建 (R2)\n");
    Aclic a;
    a.set_sourcecfg(14, Sm::Level1);
    a.set_ie(14, true);
    a.set_iprio(14, 4);
    a.set_line(14, true);
    a.sw_setip(14, false);
    CHECK(a.peek_ip(14), "line 仍高:清鏡子沒有用,要拿走鏡子照的東西");
}

int main() {
    t1_edge_basic();
    t2_arbitration();
    t3_threshold_boundary();
    t4_level_mirror();
    t5_edge_silent_loss();
    t6_winner_vanishes();
    t7_claim_clears_only_winner();
    t8_detached();
    t9_inactive();
    t10_threshold_masks_arb_not_storage();
    t11_level_sw_clear_futile();
    std::printf("\n%d checks, %d FAIL → %s\n", checks, fails, fails ? "還沒過" : "ALL PASS,回來對答案");
    return fails ? 1 : 0;
}
