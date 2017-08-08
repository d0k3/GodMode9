#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# SPECS is the directory containing the important build and link files
#---------------------------------------------------------------------------------
export TARGET	:=	GodMode9
ifeq ($(SAFEMODE),1)
	export TARGET	:=	SafeMode9
endif
BUILD		:=	build
SOURCES		:=	source source/common source/filesys source/crypto source/fatfs source/nand source/virtual source/game source/gamecart source/quicklz
DATA		:=	data
INCLUDES	:=	common source source/common source/font source/filesys source/crypto source/fatfs source/nand source/virtual source/game source/gamecart source/quicklz

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork -flto

CFLAGS	:=	-g -Wall -Wextra -Wpedantic -Wcast-align -Wno-main -O2\
			-march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math -std=gnu11\
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9

CFLAGS	+=	-DBUILD_NAME="\"$(TARGET) (`date +'%Y/%m/%d'`)\""

ifeq ($(FONT),ORIG)
CFLAGS	+=	-DFONT_ORIGINAL
else ifeq ($(FONT),6X10)
CFLAGS	+=	-DFONT_6X10
else ifeq ($(FONT),ACORN)
CFLAGS	+=	-DFONT_ACORN
else ifeq ($(FONT),GB)
CFLAGS	+=	-DFONT_GB
else
CFLAGS	+=	-DFONT_6X10
endif

ifeq ($(SAFEMODE),1)
	CFLAGS += -DSAFEMODE
endif

ifeq ($(SWITCH_SCREENS),1)
	CFLAGS += -DSWITCH_SCREENS
endif

ifneq ("$(wildcard $(CURDIR)/data/aeskeydb.bin)","")
	CFLAGS += -DHARDCODE_KEYS
endif

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-T../link.ld -nostartfiles -g $(ARCH) -Wl,-Map,$(TARGET).map

LIBS	:=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT_D	:=	$(CURDIR)/output
export OUTPUT	:=	$(OUTPUT_D)/$(TARGET)
export RELEASE	:=	$(CURDIR)/release

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/gm9*.*))) \
				$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/aeskeydb.bin)))
ifeq ($(SAFEMODE),1)
	BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/sm9*.*))) \
					$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/aeskeydb.bin)))
endif

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
export HFILES	:= $(addsuffix .h,$(subst .,_,$(BINFILES)))

export OFILES_BIN	:= $(addsuffix .o,$(BINFILES))
export OFILES_SOURCES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: common clean all firm binary screeninit release

#---------------------------------------------------------------------------------
all: firm

common:
	@[ -d $(OUTPUT_D) ] || mkdir -p $(OUTPUT_D)
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)

screeninit:
	@$(MAKE) --no-print-directory dir_out=$(OUTPUT_D) -C screeninit

binary: common
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

firm: binary screeninit
	firmtool build $(OUTPUT).firm -n 0x08006000 -A 0x08006000 -D $(OUTPUT).bin $(OUTPUT_D)/screeninit.elf -C NDMA XDMA -S nand-retail -g
	firmtool build $(OUTPUT)_dev.firm -n 0x08006000 -A 0x08006000 -D $(OUTPUT).bin $(OUTPUT_D)/screeninit.elf -C NDMA XDMA -S nand-dev -g

release:
	@rm -fr $(BUILD) $(OUTPUT_D) $(RELEASE)
	@$(MAKE) --no-print-directory binary
	@$(MAKE) --no-print-directory firm
	@[ -d $(RELEASE) ] || mkdir -p $(RELEASE)
	@cp $(OUTPUT).firm $(RELEASE)
	@cp $(CURDIR)/README.md $(RELEASE)
	@cp $(CURDIR)/HelloScript.gm9 $(RELEASE)
	@cp -R $(CURDIR)/resources/gm9 $(RELEASE)/gm9
	@-7z a $(RELEASE)/$(TARGET)-`date +'%Y%m%d-%H%M%S'`.zip $(RELEASE)/*

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@-$(MAKE) clean --no-print-directory -C screeninit
	@rm -fr $(BUILD) $(OUTPUT_D) $(RELEASE)


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).bin	:	$(OUTPUT).elf
$(OFILES_SOURCES) : $(HFILES)
$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
%.bin: %.elf
	@$(OBJCOPY) --set-section-flags .bss=alloc,load,contents -O binary $< $@
	@echo built ... $(notdir $@)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%_qlz.h %.qlz.o: %.qlz
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
#---------------------------------------------------------------------------------
%_bin.h %.bin.o: %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)


#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
