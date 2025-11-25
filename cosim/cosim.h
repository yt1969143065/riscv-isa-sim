#ifndef __COSIM_H
#define __COSIM_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "cosim-def.h"
#include "sim.h"
#include "mmu.h"

enum { REF_TO_ENV, ENV_TO_REF };

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


class CosimRef : public sim_t{
public:
  CosimRef();

  void get_regs(diff_context_t *ctx);
  void set_regs(diff_context_t *ctx, bool on_demand);
  void memcpy_from_dut(reg_t dest, void* src, size_t n);

private:
  const cfg_t *create_cfg();
};
#endif

