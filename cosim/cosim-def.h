#ifndef __COSIM_DEF_H
#define __COSIM_DEF_H

#define CONFIG_DIFF_ISA_STRING "rv64imafdcvzicsr_zifencei_zihpm_zicntr_zba_zbb_zbs_zfh_zvfh"
#define CONFIG_MEMORY_SIZE     (2 * 1024 * 1024 * 1024UL)
#define CONFIG_PMP_NUM         16
#define CONFIG_PMP_GRAN        12
#define CONFIG_MISALIGNED      true
//used by mmu
#define CONFIG_MAX_PADDR_BITS  40
#define CONFIG_MMU_CAPABILITY  IMPL_MMU_SV39

#endif

