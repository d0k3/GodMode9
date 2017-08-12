#pragma once

/* Status Register flags */
#define SR_USR_MODE (0x10)
#define SR_FIQ_MODE (0x11)
#define SR_IRQ_MODE (0x12)
#define SR_SVC_MODE (0x13)
#define SR_ABT_MODE (0x17)
#define SR_UND_MODE (0x1B)
#define SR_SYS_MODE (0x1F)
#define SR_PMODE_MASK (0x1F)

#define SR_THUMB  (1<<5)
#define SR_FIQ    (1<<6)
#define SR_IRQ    (1<<7)

#ifdef ARM9
#define CR_ENABLE_MPU    (1<<0)
#define CR_ENABLE_BIGEND (1<<7)
#define CR_ENABLE_DCACHE (1<<2)
#define CR_ENABLE_ICACHE (1<<12)
#define CR_ENABLE_DTCM   (1<<16)
#define CR_ENABLE_ITCM   (1<<18)
#define CR_ALT_VECTORS   (1<<13)
#define CR_CACHE_RROBIN  (1<<14)
#define CR_DISABLE_TBIT  (1<<15)
#define CR_DTCM_LMODE    (1<<17)
#define CR_ITCM_LMODE    (1<<19)
#endif
