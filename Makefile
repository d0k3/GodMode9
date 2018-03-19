
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools
include Makefile.common

# Base definitions
export VERSION	:=	$(shell git describe --tags --abbrev=8)
export DBUILTS	:=	$(shell date +'%Y%m%d%H%M%S')
export DBUILTL  :=	$(shell date +'%Y-%m-%d %H:%M:%S')

export OUTDIR := output
export RELDIR := release

# Definitions for initial RAM disk
VRAM_OUT    := $(OUTDIR)/vram0.tar
VRAM_DATA   := data
VRAM_FLAGS  := --make-new --path-limit 99 --size-limit 3145728

ifeq ($(OS),Windows_NT)
    PY3 := py -3
else
    PY3 := python3
endif

# Definitions for ARM binaries
export INCLUDE := -I"$(shell pwd)/common"

export ASFLAGS := -g -x assembler-with-cpp $(INCLUDE)
export CFLAGS  := -DDBUILTS="\"$(DBUILTS)\"" -DDBUILTL="\"$(DBUILTL)\"" -DVERSION="\"$(VERSION)\"" -DFLAVOR="\"$(FLAVOR)\"" \
                  -g -O2 -Wall -Wextra -Wpedantic -Wcast-align -Wno-main \
                  -fomit-frame-pointer -ffast-math -std=gnu11 \
                  -Wno-unused-function $(INCLUDE)
export LDFLAGS := -Tlink.ld -nostartfiles -Wl,--gc-sections,-z,max-page-size=512
ELF := arm9/arm9.elf arm11/arm11.elf

.PHONY: all firm vram0 elf release clean
all: firm

clean:
	@set -e; for elf in $(ELF); do \
	    $(MAKE) --no-print-directory -C $$(dirname $$elf) clean; \
	done
	@rm -rf $(OUTDIR) $(RELDIR) $(FIRM) $(FIRMD) $(VRAM_OUT)

release: clean
	@$(MAKE) --no-print-directory firm
	@$(MAKE) --no-print-directory firm NTRBOOT=1

	@mkdir -p $(RELDIR)
	@mkdir -p $(RELDIR)/ntrboot
	@mkdir -p $(RELDIR)/elf

	@cp $(FIRM) $(RELDIR)
	@cp $(OUTDIR)/$(FLAVOR)_ntr.firm $(RELDIR)/ntrboot/
	@cp $(OUTDIR)/$(FLAVOR)_ntr_dev.firm $(RELDIR)/ntrboot/
	@cp $(OUTDIR)/$(FLAVOR).firm $(RELDIR)/
	@cp $(OUTDIR)/$(FLAVOR)_dev.firm $(RELDIR)/
	@cp $(ELF) $(RELDIR)/elf
	@cp $(CURDIR)/README.md $(RELDIR)
	@cp -R $(CURDIR)/resources/gm9 $(RELDIR)/gm9
	@cp -R $(CURDIR)/resources/sample $(RELDIR)/sample

	@-7za a $(RELDIR)/$(FLAVOR)-$(VERSION)-$(DBUILTS).zip ./$(RELDIR)/*

vram0:
	@mkdir -p "$(OUTDIR)"
	@echo "Creating $(VRAM_OUT)"
	@$(PY3) utils/add2tar.py $(VRAM_FLAGS) $(VRAM_OUT) $(shell ls -d $(README) $(SPLASH) $(VRAM_DATA)/*)

%.elf: .FORCE
	@echo "Building $@"
	@$(MAKE) --no-print-directory -C $(@D)

firm: $(ELF) vram0
	@test `wc -c <$(VRAM_OUT)` -le 3145728
	@mkdir -p $(call dirname,"$(FIRM)") $(call dirname,"$(FIRMD)")
	@echo "[FIRM] $(FIRM)"
	@firmtool build $(FIRM) $(FTFLAGS) -g -A 0x18000000 -D $(ELF) $(VRAM_OUT) -C NDMA XDMA memcpy
	@echo "[FIRM] $(FIRMD)"
	@firmtool build $(FIRMD) $(FTDFLAGS) -g -A 0x18000000 -D $(ELF) $(VRAM_OUT)  -C NDMA XDMA memcpy

.FORCE:
