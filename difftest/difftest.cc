#include "decode_macros.h"
#include "difftest.h"
#include "disasm.h"
#include "softfloat.h"

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
  // INFO: SPIKE's state_t struct and its reset function sucks.
  for (int i = 0; i < NXPR; i++) {
    ctx->gpr[i] = state->XPR[i];
  }
  ctx->pc = state->pc;
  //F
  for (int i = 0; i < NFPR; i++) {
    ctx->fpr[i] = unboxF64(state->FPR[i]);
  }
  ctx->fcsr = state->fflags->read() | (state->frm->read() << FSR_RD_SHIFT);
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
  ctx->vstart     = vstate.vstart->read();
  ctx->vxsat      = vstate.vxsat->read();
  ctx->vxrm       = vstate.vxrm->read();
  ctx->vcsr       = state->csrmap[CSR_VCSR]->read();
  ctx->vl         = vstate.vl->read();
  ctx->vtype      = vstate.vtype->read();
  ctx->vlenb      = vstate.vlenb;
  //M-mode
  ctx->mstatus = state->mstatus->read();
  ctx->mepc = state->mepc->read();
  ctx->mtval = state->mtval->read();
  ctx->mtvec = state->mtvec->read();
  ctx->mcause = state->mcause->read();
  ctx->mip = state->mip->read();
  ctx->mie = state->mie->read();
  ctx->mideleg = state->mideleg->read();
  ctx->medeleg = state->medeleg->read();
  ctx->mscratch = state->mscratch->read();
  //S-mode
  ctx->sstatus = state->nonvirtual_sstatus->read();
  ctx->sepc = state->nonvirtual_sepc->read();
  ctx->stval = state->nonvirtual_stval->read();
  ctx->stvec = state->nonvirtual_stvec->read();
  ctx->scause = state->nonvirtual_scause->read();
  ctx->sscratch = state->nonvirtual_sscratch->read();
  ctx->satp = state->nonvirtual_satp->read();
}

//difftest interface --> spike
void DifftestRef::set_regs(diff_context_t *ctx, bool on_demand) {
  for (int i = 0; i < NXPR; i++) {
    if (!on_demand || state->XPR[i] != ctx->gpr[i]) {
      state->XPR.write(i, ctx->gpr[i]);
    }
  }
  if (!on_demand || state->pc != ctx->pc) {
    state->pc = ctx->pc;
  }
  //F
  for (int i = 0; i < NFPR; i++) {
    if (!on_demand || unboxF64(state->FPR[i]) != ctx->fpr[i]) {
      state->FPR.write(i, freg(f64(ctx->fpr[i])));
    }
  }
  if (!on_demand || (state->fflags->read() | state->frm->read() << FSR_RD_SHIFT) != ctx->fcsr) {
    state->fflags->write_raw(ctx->fcsr & FSR_AEXC);
    state->frm->write_raw((ctx->fcsr & FSR_RD) >> FSR_RD_SHIFT);
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
  /***********************************************************************************/
  if (!on_demand || vstate.vstart->read() != ctx->vstart) {
    vstate.vstart->write_raw(ctx->vstart);
  }
  if (!on_demand || vstate.vxsat->read() != ctx->vxsat) {
    vstate.vxsat->write_raw(ctx->vxsat);
  }
  if (!on_demand || vstate.vxrm->read() != ctx->vxrm) {
    vstate.vxrm->write_raw(ctx->vxrm);
  }
  /******************************Don't need write vcsr**********************************/
  // if (!on_demand || state->csrmap[CSR_VCSR]->read() !=ctx->vcsr) {
  //   csrmap[CSR_VCSR]->write(ctx->vcsr);
  // }
  if (!on_demand || vstate.vl->read() != ctx->vl) {
    vstate.vl->write_raw(ctx->vl);
  }
  if (!on_demand || vstate.vtype->read() != ctx->vtype) {
    vstate.vtype->write_raw(ctx->vtype);
  }
  if (!on_demand || vstate.vlenb != ctx->vlenb) {
    vstate.vlenb = ctx->vlenb;
  }
  //M-mode
  if (!on_demand || state->mstatus->read() != ctx->mstatus) {
    state->mstatus->write(ctx->mstatus);
  }
  if (!on_demand || state->mepc->read() != ctx->mepc) {
    state->mepc->write(ctx->mepc);
  }
  if (!on_demand || state->mtval->read() != ctx->mtval) {
    state->mtval->write(ctx->mtval);
  }
  if (!on_demand || state->mtvec->read() != ctx->mtvec) {
    state->mtvec->write(ctx->mtvec);
  }
  if (!on_demand || state->mcause->read() != ctx->mcause) {
    state->mcause->write(ctx->mcause);
  }
  if (!on_demand || state->mip->read() != ctx->mip) {
    state->mip->write(ctx->mip);
  }
  if (!on_demand || state->mie->read() != ctx->mie) {
    state->mie->write(ctx->mie);
  }
  if (!on_demand || state->mideleg->read() != ctx->mideleg) {
    state->mideleg->write(ctx->mideleg);
  }
  if (!on_demand || state->medeleg->read() != ctx->medeleg) {
    state->medeleg->write(ctx->medeleg);
  }
  if (!on_demand || state->mscratch->read() != ctx->mscratch) {
    state->mscratch->write(ctx->mscratch);
  }
  //S-mode
  if (!on_demand || state->nonvirtual_sstatus->read() != ctx->sstatus) {
    state->nonvirtual_sstatus->write(ctx->sstatus);
  }
  if (!on_demand || state->nonvirtual_sepc->read() != ctx->sepc) {
    state->nonvirtual_sepc->write(ctx->sepc);
  }
  if (!on_demand || state->stval->read() != ctx->stval) {
    state->stval->write(ctx->stval);
  }
  if (!on_demand || state->nonvirtual_stvec->read() != ctx->stvec) {
    state->nonvirtual_stvec->write(ctx->stvec);
  }
  if (!on_demand || state->nonvirtual_scause->read() != ctx->scause) {
    state->nonvirtual_scause->write(ctx->scause);
  }
  if (!on_demand || state->nonvirtual_sscratch->read() != ctx->sscratch) {
    state->nonvirtual_sscratch->write(ctx->sscratch);
  }
  if (!on_demand || state->nonvirtual_satp->read() != ctx->satp) {
    state->nonvirtual_satp->write(ctx->satp);
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

void DifftestRef::pmpcpy(reg_t* dut, bool direction) {
  for (int i = 0; i < CONFIG_PMP_NUM; i++) {
    if (direction == DIFFTEST_TO_REF) {
      state->pmpaddr[i]->write(dut[i]);
    }else{
      dut[i] = state->pmpaddr[i]->read();
    }
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
  cfg->force_override = true;
  return cfg;
}

const std::vector<std::pair<reg_t, abstract_device_t*>> DifftestRef::create_devices() {
  return std::vector<std::pair<reg_t, abstract_device_t*>>{ };
}

void DifftestRef::pmp_cfg_cpy(reg_t *dut, bool direction) {
  auto xlen = p->get_isa().get_max_xlen();
  for (int i = 0; i < state->max_pmp; i += xlen / 8) {
    reg_t addr = CSR_PMPCFG0 + i / 4;
    if (direction == DIFFTEST_TO_REF) {
      state->csrmap[addr]->write(dut[addr]);
    } else {
      dut[addr] = state->csrmap[addr]->read();
    }
  }
}

void DifftestRef::pmpcpy(reg_t* dut, bool direction) {
  for (int i = 0; i < CONFIG_PMP_NUM; i++) {
    if (direction == DIFFTEST_TO_REF) {
      state->pmpaddr[i]->write(dut[i]);
    }else{
      dut[i] = state->pmpaddr[i]->read();
    }
  }
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
    nullptr,
    // const char *log_path
    nullptr,
    //bool dtb_enabled, const char *dtb_file, bool socket_enabled, FILE *cmd_file
    false, nullptr, false, nullptr
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

void difftest_pmp_cfg_cpy(void *dut, bool direction) {
  ref->pmp_cfg_cpy((reg_t*)dut, direction);
}

void difftest_pmpcpy(void *dut, bool direction) {
  ref->pmpcpy((reg_t*)dut, direction);
}

void update_dynamic_config(void* config) {
  ref->update_dynamic_config(config);
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
