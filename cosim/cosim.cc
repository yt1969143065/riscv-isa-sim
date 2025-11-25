#include "decode_macros.h"
#include "cosim.h"
#include "disasm.h"
#include "softfloat.h"

static debug_module_config_t cosim_dm_config = {
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

static CosimRef *ref = nullptr;
static processor_t *p = nullptr;
static state_t *state = nullptr;

CosimRef::CosimRef() :
  sim_t(create_cfg(), //const cfg_t *cfg
        false, //bool halted
        std::vector<std::pair<reg_t, abstract_mem_t*>>{}, //std::vector<std::pair<reg_t, abstract_mem_t*>> mems
        std::vector<device_factory_sargs_t> {}, //const std::vector<device_factory_sargs_t>& plugin_device_factories
        std::vector<std::string>{""}, //const std::vector<std::string>& args
        cosim_dm_config, //const debug_module_config_t &dm_config
        nullptr, //const char *log_path
        false, //bool dtb_enabled
        nullptr, //const char *dtb_file
        false, //bool socket_enabled
        nullptr, //FILE *cmd_file
        {}) //std::optional<unsigned long long> instruction_limit
{
  add_device(DRAM_BASE, std::shared_ptr<mem_t> (new mem_t(CONFIG_DRAM_SIZE)));
  p = get_core(0);
  state = p->get_state();
  
  p->set_mmu_capability(CONFIG_MMU_CAPABILITY);
  p->reset();
  
}

//REF --> ENV
void CosimRef::get_regs(diff_context_t *ctx) {
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

//ENV --> REF
void CosimRef::set_regs(diff_context_t *ctx, bool on_demand) {
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

void CosimRef::memcpy_from_dut(reg_t dest, void* src, size_t n) {
  mmio_store(dest, n, (const uint8_t *)src);
}

const cfg_t *CosimRef::create_cfg() {
  auto const cfg = new cfg_t();
  cfg->initrd_bounds = std::make_pair(0, 0);    //std::pair<reg_t, reg_t> initrd_bounds
  cfg->bootargs = nullptr;                      //const char * bootargs
  cfg->isa = DEFAULT_ISA;                       //const char * isa
  cfg->priv = DEFAULT_PRIV;                     //const char * priv
  cfg->misaligned = CONFIG_MISALIGNED;          //bool misaligned
  cfg->endianness = endianness_little;          //endianness_t endianness
  cfg->pmpregions = CONFIG_PMP_NUM;             //reg_t pmpregions
  cfg->pmpgranularity = (1 << CONFIG_PMP_GRAN); //reg_t pmpgranularity
  cfg->mem_layout = std::vector<mem_cfg_t>{};   //std::vector<mem_cfg_t> mem_layout
  cfg->start_pc = std::optional<reg_t>{};       //std::optional<reg_t> start_pc
  cfg->hartids = std::vector<size_t>{0};        //std::vector<size_t> hartids 
  cfg->real_time_clint = false;                 //bool real_time_clint
  cfg->trigger_count = 0;                       //reg_t trigger_count
  cfg->cache_blocksz = 0;                       //reg_t cache_blocksz;
  cfg->external_simulator = std::optional<abstract_sim_if_t*>{};//std::optional<abstract_sim_if_t*> external_simulator;
  return cfg;
}

// Following are the interfaces for co-simulation with other designs

extern "C" {

void cosim_memcpy(uint64_t addr, void *buf, size_t n, bool direction) {
  if (direction == ENV_TO_REF) {
    ref->memcpy_from_dut(addr, buf, n);
  } else {
    printf("cosim_memcpy with REF_TO_ENV is not supported yet\n");
    fflush(stdout);
    assert(0);
  }
}

void cosim_regcpy(diff_context_t* dut, bool direction, bool on_demand) {
  if (direction == ENV_TO_REF) {
    ref->set_regs(dut, on_demand);
  } else {
    ref->get_regs(dut);
  }
}

void ref_exec(uint64_t n) {
  p->step(n);
}

void ref_init(int port) {
  ref = new CosimRef;
}

void ref_close() {
  delete ref;
}

}
