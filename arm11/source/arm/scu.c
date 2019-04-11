#include <types.h>
#include <arm.h>

#define REG_SCU_CNT	(*REG_ARM_PMR(0x00, u32))
#define REG_SCU_CFG	(*REG_ARM_PMR(0x04, u32))
#define REG_SCU_CPU	(*REG_ARM_PMR(0x08, u32))
#define REG_SCU_INV	(*REG_ARM_PMR(0x0C, u32))

void SCU_Init(void)
{
	REG_SCU_CNT = 0x1FFE;
	REG_SCU_INV = 0xFFFF;
	REG_SCU_CNT = 0x3FFF;
}
