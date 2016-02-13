#ifdef EXEC_BOOTSTRAP

.section ".init"
.global _start
.extern main
.align 4
.arm

#define SIZE_32KB  0b01110
#define SIZE_128KB 0b10000
#define SIZE_512KB 0b10010
#define SIZE_2MB   0b10100
#define SIZE_128MB 0b11010
#define SIZE_256MB 0b11011
#define SIZE_4GB   0b11111

@ Makes a MPU partition value
#define MAKE_PARTITION(offset, size_enum) \
    (((offset) >> 12 << 12) | ((size_enum) << 1) | 1)


_start:
    b _init

    @ required, don't move :)
    @ will be set to FIRM ARM9 entry point by BRAHMA
    arm9ep_backup:  .long 0xFFFF0000

_mpu_partition_table:
    .word MAKE_PARTITION(0x00000000, SIZE_4GB)   @ 0: Background region
    .word MAKE_PARTITION(0x00000000, SIZE_128MB) @ 1: Instruction TCM (mirrored every 32KB)
    .word MAKE_PARTITION(0x08000000, SIZE_2MB)   @ 2: ARM9 internal memory
    .word MAKE_PARTITION(0x10000000, SIZE_128MB) @ 3: IO region
    .word MAKE_PARTITION(0x18000000, SIZE_128MB) @ 4: external device memory
    .word MAKE_PARTITION(0x1FF80000, SIZE_512KB) @ 5: AXI WRAM
    .word MAKE_PARTITION(0x20000000, SIZE_256MB) @ 6: FCRAM
    .word 0                                      @ 7: Unused

_populate_mpu:
    push {r4-r5, lr}
    ldr r4, =_mpu_partition_table

    ldr r5, [r4, #0x0]        @ mmu_partition_table[0] load
    mcr p15, 0, r5, c6, c0, 0 @ mmu_partition_table[0] write
    ldr r5, [r4, #0x4]
    mcr p15, 0, r5, c6, c1, 0
    ldr r5, [r4, #0x8]
    mcr p15, 0, r5, c6, c2, 0
    ldr r5, [r4, #0xC]
    mcr p15, 0, r5, c6, c3, 0
    ldr r5, [r4, #0x10]
    mcr p15, 0, r5, c6, c4, 0
    ldr r5, [r4, #0x14]
    mcr p15, 0, r5, c6, c5, 0
    ldr r5, [r4, #0x18]
    mcr p15, 0, r5, c6, c6, 0
    ldr r5, [r4, #0x1C]
    mcr p15, 0, r5, c6, c7, 0

    @ Give read/write access to all the memory regions
    ldr r5, =0x03333333
    mcr p15, 0, r5, c5, c0, 2 @ data access
    ldr r5, =0x03300330
    mcr p15, 0, r5, c5, c0, 3 @ instruction access

    mov r5, #0x66
    mcr p15, 0, r5, c2, c0, 0  @ data cachable
    mcr p15, 0, r5, c2, c0, 1  @ instruction cachable

    mov r5, #0x10
    mcr p15, 0, r5, c3, c0, 0  @ data bufferable

    pop {r4-r5, pc}

_enable_caches:
    push {r4-r5, lr}

    bl _populate_mpu
    mov r5, #0
    mcr p15, 0, r5, c7, c5, 0  @ flush I-cache
    mcr p15, 0, r5, c7, c6, 0  @ flush D-cache

    mrc p15, 0, r4, c1, c0, 0
    orr r4, r4, #(1<<12)       @ instruction cache enable
    orr r4, r4, #(1<<2)        @ data cache enable
    orr r4, r4, #(1<<0)        @ mpu enable
    mcr p15, 0, r4, c1, c0, 0

    pop {r4-r5, pc}

_init:
    push {r0-r12, lr}

    bl _enable_caches
    bl main

    mrc p15, 0, r4, c1, c0, 0
    bic r4, r4, #(1<<0)        @ mpu disable
    mcr p15, 0, r4, c1, c0, 0

    pop {r0-r12, lr}

    @ return control to FIRM
    ldr pc, arm9ep_backup

#endif // EXEC_BOOTSTRAP
