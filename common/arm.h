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

#define SR_THUMB  (1 << 5)
#define SR_FIQ    (1 << 6)
#define SR_IRQ    (1 << 7)
