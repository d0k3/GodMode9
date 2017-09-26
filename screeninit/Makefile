rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

name := $(shell basename $(CURDIR))

dir_source := source
dir_build := build
dir_out := ../$(dir_build)

ARCH    := -DARM11 -mcpu=mpcore -mfloat-abi=soft -marm -mno-thumb-interwork
INCLUDE := -I$(dir_source) -I../common

ASFLAGS := $(ARCH) -x assembler-with-cpp $(INCLUDE)
CFLAGS  := $(ARCH) -Wall -Wextra -MMD -MP -fno-builtin \
           -Wno-unused-function -Wno-unused-variable \
           -std=c11 -O2 -flto -ffast-math -Wno-main $(INCLUDE)
LDFLAGS := -nostdlib -nostartfiles -Wl,-z,max-page-size=512

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
	$(CC) -c $(CFLAGS) -o $@ $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(CC) -c $(ASFLAGS) -o $@ $<
