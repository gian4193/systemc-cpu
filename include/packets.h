#pragma once
#include <systemc.h>
#include <cstdint>

// =======================================
// IF/ID 之間 packet (frontend → decode)
// =======================================
struct IfId {
    uint32_t pc;
    uint32_t inst;
};
inline bool operator==(const IfId& a, const IfId& b) {
    return a.pc == b.pc && a.inst == b.inst;
}
inline std::ostream& operator<<(std::ostream& os, const IfId& p) {
    os << "{pc=" << p.pc << ", inst=0x" << std::hex << p.inst << std::dec << "}";
    return os;
}
inline void sc_trace(sc_trace_file* tf, const IfId& p, const std::string& name) {
    sc_trace(tf, p.pc, name + ".pc");
    sc_trace(tf, p.inst, name + ".inst");
}

// =======================================
// ICache ↔ MainMem packets
// =======================================
struct LineRequest {
    uint32_t pc;   // line-aligned PC (bits[5:0]=0)
};
inline bool operator==(const LineRequest& a, const LineRequest& b) {
    return a.pc == b.pc;
}
inline std::ostream& operator<<(std::ostream& os, const LineRequest& r) {
    os << "{req pc=" << r.pc << "}";
    return os;
}
inline void sc_trace(sc_trace_file* tf, const LineRequest& r, const std::string& name) {
    sc_trace(tf, r.pc, name + ".pc");
}

struct LineResponse {
    uint32_t pc;        // 哪個 line 回來
    uint32_t data[16];  // 整個 line 16 條 inst
};
inline bool operator==(const LineResponse& a, const LineResponse& b) {
    if (a.pc != b.pc) return false;
    for (int i = 0; i < 16; i++) if (a.data[i] != b.data[i]) return false;
    return true;
}
inline std::ostream& operator<<(std::ostream& os, const LineResponse& r) {
    os << "{resp pc=" << r.pc
       << " data[0]=0x" << std::hex << r.data[0] << std::dec << "}";
    return os;
}
inline void sc_trace(sc_trace_file* tf, const LineResponse& r, const std::string& name) {
    sc_trace(tf, r.pc, name + ".pc");
}
