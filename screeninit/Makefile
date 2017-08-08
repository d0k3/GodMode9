rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

name := $(shell basename $(CURDIR))

dir_source := source
dir_build := build
dir_out := ../$(dir_build)

ASFLAGS := -mcpu=mpcore -mfloat-abi=soft
CFLAGS  := -DARM11 -Wall -Wextra -MMD -MP -marm -mno-thumb-interwork \
           $(ASFLAGS) -fno-builtin -std=c11 -Wno-main -O2 -flto -ffast-math \
           -I$(dir_source) -I../common
LDFLAGS := -nostdlib -nostartfiles

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

.PHONY: all
all: $(dir_out)/$(name).bin

.PHONY: clean
clean:
	@rm -rf $(dir_build)

$(dir_out)/$(name).bin: $(dir_out)/$(name).elf
	$(OBJCOPY) -S -O binary $< $@

$(dir_out)/$(name).elf: $(objects)
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
