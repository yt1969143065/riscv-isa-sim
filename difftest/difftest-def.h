#ifndef __DIFFTEST_DEF_H
#define __DIFFTEST_DEF_H

//#define CONFIG_DIFF_ISA_STRING "rv64imafdcvzicsr_zifencei_zihpm_zicntr_zfh_zvfh_zba_zbb_zbs"
#define CONFIG_DIFF_ISA_STRING "rv64imafdcvzicsr_zifencei_zihpm_zicntr_zba_zbb_zbs_zfh_zvfh"
#define CONFIG_MEMORY_SIZE     (2 * 1024 * 1024 * 1024UL)
#define CONFIG_FLASH_BASE      0x10000000UL
#define CONFIG_FLASH_SIZE      0x10000UL
#define CONFIG_PMP_NUM         16
#define CONFIG_PMP_MAX_NUM     16
#define CONFIG_PMP_GRAN        12
#define CONFIG_TRIGGER_NUM     4
#define CONFIG_MAX_PADDR_BITS  40
#define CONFIG_MMU_CAPABILITY  IMPL_MMU_SV39
#define CONFIG_MISALIGNED      true

#endif

