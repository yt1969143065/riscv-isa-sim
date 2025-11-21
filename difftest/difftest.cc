#include "decode_macros.h"
#include "difftest.h"
#include "disasm.h"
#include "softfloat.h"

static debug_module_config_t difftest_dm_config = {
  .progbufsize = 2,
  .max_sba_data_width = 0,
  .require_authentication = false,
  .abstract_rti = 0,
  .support_hasel = true,
  .support_abstract_csr_access = true,
  .support_abstract_fpr_access = true,
  .support_haltgroups = true,
  .support_impebreak = false
};

extern std::vector<std::pair<reg_t, abstract_mem_t*>> make_mems(const std::vector<mem_cfg_t> &layout);

static DifftestRef *ref = nullptr;
static size_t overrided_mem_size = 0;
static size_t overrided_mhartid = 0;

DifftestRef::DifftestRef() :
  cfg(create_cfg()),
  mems(make_mems(cfg->mem_layout)),
  plugin_devices(create_devices()),
  sim(create_sim(cfg)),
  p(sim->get_core(0UL)),
  state(p->get_state()) {
#if CONFIG_PMP_NUM > 0
  p->set_pmp_granularity(1 << CONFIG_PMP_GRAN);
#endif
}

DifftestRef::~DifftestRef() {
  delete cfg;
  for (const auto& pair : mems) {
    delete pair.second;
  }
  delete sim;
}

void DifftestRef::step(uint64_t n) {
  sim->step(n);
}
 
//spike --> difftest interface
void DifftestRef::get_regs(diff_context_t *ctx) {
  ctx->pc = state->pc;
  for (int i = 0; i < NXPR; i++) {
    ctx->gpr[i] = state->XPR[i];
  }
  //F
  for (int i = 0; i < NFPR; i++) {
    ctx->fpr[i] = unboxF64(state->FPR[i]);
  }
  //V
  auto& vstate = p->VU;
  /*******************************ONLY FOR VLEN=256,ELEN=64*******************************************/
  for(int i = 0; i < NVPR; i++){
    auto vReg_Val0 = vstate.elt<uint64_t>(i, 0,false);
    auto vReg_Val1 = vstate.elt<uint64_t>(i, 1,false);
    auto vReg_Val2 = vstate.elt<uint64_t>(i, 2,false);
    auto vReg_Val3 = vstate.elt<uint64_t>(i, 3,false);
    ctx->vr[i]._64[0] = vReg_Val0;
    ctx->vr[i]._64[1] = vReg_Val1;
    ctx->vr[i]._64[2] = vReg_Val2;
    ctx->vr[i]._64[3] = vReg_Val3;
  }
  /***************************************************************************************************/
}

//difftest interface --> spike
void DifftestRef::set_regs(diff_context_t *ctx, bool on_demand) {
  if (!on_demand || state->pc != ctx->pc) {
    state->pc = ctx->pc;
  }
  for (int i = 0; i < NXPR; i++) {
    if (!on_demand || state->XPR[i] != ctx->gpr[i]) {
      state->XPR.write(i, ctx->gpr[i]);
    }
  }
  //F
  for (int i = 0; i < NFPR; i++) {
    if (!on_demand || unboxF64(state->FPR[i]) != ctx->fpr[i]) {
      state->FPR.write(i, freg(f64(ctx->fpr[i])));
    }
  }
  //V
  auto& vstate = p->VU;
  /**********************ONLY FOR VLEN=256,ELEN=64************************************/
  for (int i = 0; i < NVPR; i++) {
    auto &vReg_Val0 = p->VU.elt<uint64_t>(i, 0, true);
    auto &vReg_Val1 = p->VU.elt<uint64_t>(i, 1, true);
    auto &vReg_Val2 = p->VU.elt<uint64_t>(i, 2, true);
    auto &vReg_Val3 = p->VU.elt<uint64_t>(i, 3, true);
    if (!on_demand || vReg_Val0 != ctx->vr[i]._64[0]) {
      vReg_Val0 = ctx->vr[i]._64[0];
    }
    if(!on_demand || vReg_Val1 != ctx->vr[i]._64[1]){
      vReg_Val1 = ctx->vr[i]._64[1];
    }
    if(!on_demand || vReg_Val2 != ctx->vr[i]._64[2]){
      vReg_Val2 = ctx->vr[i]._64[2];
    }
    if(!on_demand || vReg_Val3 != ctx->vr[i]._64[3]){
      vReg_Val3 = ctx->vr[i]._64[3];
    }
  }
}

void DifftestRef::memcpy_from_dut(reg_t dest, void* src, size_t n) {
  while (n) {
    char *base = sim->addr_to_mem(dest);
    size_t n_bytes = (n > PGSIZE) ? PGSIZE : n;
    memcpy(base, src, n_bytes);
    dest += PGSIZE;
    src = (char *)src + PGSIZE;
    n -= n_bytes;
  }
}

const cfg_t *DifftestRef::create_cfg() {
  auto mem_size = overrided_mem_size ? overrided_mem_size : CONFIG_MEMORY_SIZE;
  auto memory_layout = std::vector<mem_cfg_t>{
    mem_cfg_t{DRAM_BASE, mem_size},
  };
  auto const cfg = new cfg_t();
  cfg->initrd_bounds = std::make_pair(0, 0);
  cfg->bootargs = nullptr;
  cfg->isa = CONFIG_DIFF_ISA_STRING;
  cfg->priv = DEFAULT_PRIV;
  cfg->misaligned = CONFIG_MISALIGNED;
  cfg->endianness = endianness_little;
  cfg->pmpregions = CONFIG_PMP_NUM;
  cfg->mem_layout = memory_layout;
  cfg->hartids = std::vector<size_t>{overrided_mhartid};
  cfg->real_time_clint = false;
  cfg->trigger_count = CONFIG_TRIGGER_NUM;
  return cfg;
}

const std::vector<std::pair<reg_t, abstract_device_t*>> DifftestRef::create_devices() {
  return std::vector<std::pair<reg_t, abstract_device_t*>>{ };
}

sim_t *DifftestRef::create_sim(const cfg_t *cfg) {
  sim_t *s = new sim_t(
    // const cfg_t *cfg,
    cfg,
    // bool halted,
    false,
    // std::vector<std::pair<reg_t, abstract_mem_t*>> mems
    mems,
    // const std::vector<device_factory_sargs_t>& plugin_device_factories
    std::vector<device_factory_sargs_t>{},
    // const std::vector<std::string>& args
    std::vector<std::string>{},
    // const debug_module_config_t &dm_config
    difftest_dm_config,
    // const char *log_path
    nullptr,
    //bool dtb_enabled, const char *dtb_file, bool socket_enabled, FILE *cmd_file
    false, nullptr, false, nullptr,
    //std::optional<unsigned long long> instruction_limit
    {}
  );

  for (const auto& pair : plugin_devices) {
    s->add_device(pair.first, std::shared_ptr<abstract_device_t>(pair.second));
  }

  return s;
}

// Following are the interfaces for co-simulation with other designs

extern "C" {

void difftest_memcpy(uint64_t addr, void *buf, size_t n, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    ref->memcpy_from_dut(addr, buf, n);
  } else {
    printf("difftest_memcpy with DIFFTEST_TO_DUT is not supported yet\n");
    fflush(stdout);
    assert(0);
  }
}

void difftest_regcpy(diff_context_t* dut, bool direction, bool on_demand) {
  if (direction == DIFFTEST_TO_REF) {
    ref->set_regs(dut, on_demand);
  } else {
    ref->get_regs(dut);
  }
}

void difftest_exec(uint64_t n) {
  ref->step(n);
}

void difftest_init(int port) {
  ref = new DifftestRef;
}

void difftest_close() {
  delete ref;
}

}
