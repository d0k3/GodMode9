
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
VRAM_OUT   := $(OUTDIR)/vram0.tar
VRAM_DIRS  := data/
VRAM_FLAGS :=

# Definitions for ARM binaries
export INCLUDE := -I"$(shell pwd)/common"

export ASFLAGS := -g -x assembler-with-cpp $(INCLUDE)
export CFLAGS  := -DDBUILTS="\"$(DBUILTS)\"" -DDBUILTL="\"$(DBUILTL)\"" -DVERSION="\"$(VERSION)\""\
                  -g -O2 -Wall -Wextra -Wpedantic -Wcast-align -Wno-main \
                  -fomit-frame-pointer -ffast-math -std=gnu11 \
                  -Wno-unused-function $(INCLUDE)
export LDFLAGS := -Tlink.ld -nostartfiles -Wl,--gc-sections,-z,max-page-size=512
ELF := arm9/arm9.elf mpcore/mpcore.elf

.PHONY: all firm clean
all: firm

clean:
	@set -e; for elf in $(ELF); do \
	    $(MAKE) --no-print-directory -C $$(dirname $$elf) clean; \
	done
	@rm -rf $(OUTDIR) $(RELDIR) $(FIRM) $(FIRMD) $(VRAM_OUT)

release:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory firm
	@$(MAKE) --no-print-directory firm NTRBOOT=1

	@mkdir -p $(RELDIR)
	@cp $(FIRM) $(RELDIR)
	@[ -d $(RELDIR) ] || mkdir -p $(RELDIR)
	@[ -d $(RELDIR)/ntrboot ] || mkdir -p $(RELDIR)/ntrboot
	@cp $(OUTDIR)/$(FLAVOR)_ntr.firm $(RELDIR)/ntrboot/
	@cp $(OUTDIR)/$(FLAVOR)_ntr_dev.firm $(RELDIR)/ntrboot/
	@cp $(OUTDIR)/$(FLAVOR).firm $(RELDIR)/
	@cp $(OUTDIR)/$(FLAVOR)_dev.firm $(RELDIR)/
	@cp $(ELF) $(RELDIR)
	@cp $(CURDIR)/README.md $(RELDIR)
	@cp $(CURDIR)/HelloScript.gm9 $(RELDIR)
	@cp -R $(CURDIR)/resources/gm9 $(RELDIR)/gm9
	@-7z a $(RELDIR)/$(FLAVOR)-$(VERSION)-$(DBUILTS).zip $(RELDIR)/*

$(VRAM_OUT):
	@mkdir -p "$(@D)"
	@echo "Creating $@"
	@tar $(VRAM_FLAGS) cf $@ $(VRAM_DIRS)

%.elf:
	@echo "Building $@"
	@$(MAKE) --no-print-directory -C $(call dirname,"$@")

firm: $(ELF) $(VRAM_OUT)
	@mkdir -p $(call dirname,"$(FIRM)") $(call dirname,"$(FIRMD)")
	firmtool build $(FIRM) $(FTFLAGS) -g -A 0x18000000 -D $^ -C NDMA XDMA memcpy
	firmtool build $(FIRMD) $(FTDFLAGS) -g -A 0x18000000 -D $^ -C NDMA XDMA memcpy
