#pragma once
#include <array>
#include <cstdint>

/* ACLIC 最小模型 — 介面(勿改;行為定義見 SPEC.md) */
class Aclic {
public:
    static constexpr int NSRC = 32;   /* id 0 保留 =「無中斷」,有效 1..31 */

    enum class Sm : uint8_t {
        Inactive = 0, Detached = 1,
        Edge1 = 4, Edge0 = 5, Level1 = 6, Level0 = 7,
    };

    Aclic() { reset(); }
    void reset();                                   /* R7 */

    /* ---- device 側:唯一能做的動作是驅動電位 ---- */
    void set_line(int src, bool level);             /* R1 */

    /* ---- hart 側:政策設定(相當於經門牌的 CSR 存取) ---- */
    void set_sourcecfg(int src, Sm sm);
    void set_iprio(int src, uint8_t prio) { iprio_[src] = prio; }   /* 越小越優先 */
    void set_ie(int src, bool en)         { setie_[src] = en; }
    void set_threshold(uint8_t p)         { ithreshold_ = p; }      /* 0=不設限 */
    void sw_setip(int src, bool pend);              /* R2 */

    /* ---- 觀察與 claim ---- */
    uint32_t topi() const;                          /* R3+R4;const = 純函數,型別強制 */
    bool     irq_out() const { return topi() != 0; }/* R6 */
    uint32_t claim();                               /* R5:唯一有副作用的讀 */

    /* testbench 後門 */
    bool peek_ip(int src) const { return setip_[src]; }

private:
    /* TODO(1) 的 helper 宣告在這裡(例如 void mirror_(int src);) */

    std::array<Sm, NSRC>      sourcecfg_{};
    std::array<uint8_t, NSRC> iprio_{};
    std::array<bool, NSRC>    setip_{}, setie_{}, line_{};
    uint8_t                   ithreshold_ = 0;
};
