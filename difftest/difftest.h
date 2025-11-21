#ifndef __DIFFTEST_H
#define __DIFFTEST_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "difftest-def.h"
#include "sim.h"
#include "mmu.h"

enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };
#define FMT_WORD "0x%016lx"
#define DIFFTEST_REG_SIZE (sizeof(uint64_t) * 33) // GRPs + pc

typedef struct {
  uint64_t pc;
  uint64_t gpr[32];
  //F
  uint64_t fpr[32];
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
} diff_context_t;


class DifftestRef {
public:
  DifftestRef();
  ~DifftestRef();
  void step(uint64_t n);
  void get_regs(diff_context_t *ctx);
  void set_regs(diff_context_t *ctx, bool on_demand);
  void memcpy_from_dut(reg_t dest, void* src, size_t n);

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

