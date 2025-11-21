#ifndef __DIFFTEST_H
#define __DIFFTEST_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "difftest-def.h"
#include "sim.h"

enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };
#define FMT_WORD "0x%016lx"
#define DIFFTEST_REG_SIZE (sizeof(uint64_t) * 33) // GRPs + pc

typedef struct {
  uint64_t gpr[32];
  uint64_t pc;
  //F
  uint64_t fpr[32];
  uint64_t fcsr;
  //V
  #define VLEN 256
  #define VENUM64 (VLEN/64)
  #define VENUM32 (VLEN/32)
  #define VENUM16 (VLEN/16)
  #define VENUM8  (VLEN/8)

  union {
    uint64_t _64[VENUM64];
    uint32_t _32[VENUM32];
    uint16_t _16[VENUM16];
    uint8_t  _8[VENUM8];
  } vr[32];

  uint64_t vstart;
  uint64_t vxsat;
  uint64_t vxrm;
  uint64_t vcsr;
  uint64_t vl;
  uint64_t vtype;
  uint64_t vlenb;
  //M-mode
  uint64_t mstatus;
  uint64_t mepc;
  uint64_t mtval;
  uint64_t mtvec;
  uint64_t mcause;
  uint64_t mip;
  uint64_t mie;
  uint64_t mideleg;
  uint64_t medeleg;
  uint64_t mscratch;
  //S-mode
  uint64_t sstatus;
  uint64_t sepc;
  uint64_t stval;
  uint64_t stvec;
  uint64_t scause;
  uint64_t sscratch;
  uint64_t satp;
} diff_context_t;


class DifftestRef {
public:
  DifftestRef();
  ~DifftestRef();
  void step(uint64_t n);
  void get_regs(diff_context_t *ctx);
  void set_regs(diff_context_t *ctx, bool on_demand);
  void memcpy_from_dut(reg_t dest, void* src, size_t n);
  void pmp_cfg_cpy(reg_t *dut, bool direction);
  void pmpcpy(reg_t *dut, bool direction);
  void update_dynamic_config(void* config) {
    p->enable_log_commits();
  }

private:
  const cfg_t *cfg;
  const std::vector<std::pair<reg_t, abstract_mem_t*>> mems;
  const std::vector<std::pair<reg_t, abstract_device_t*>> plugin_devices;
  sim_t * const sim;
  processor_t * const p;
  state_t * const state;

  const cfg_t *create_cfg();
  const std::vector<std::pair<reg_t, abstract_device_t*>> create_devices();
  sim_t *create_sim(const cfg_t *cfg);
};
#endif

