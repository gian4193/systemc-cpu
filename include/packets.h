#pragma once

// TODO: define inter-stage packet structs (IfId, IdEx, BTBLookup, ...)
#include <systemc.h>
#include <cstdint>

// IF/ID 之間 packet
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

// 後續會加 IdEx、BTBLookup ... 等