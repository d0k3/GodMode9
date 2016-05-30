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
ifeq ($(MODE),safe)
	export TARGET	:=	SafeMode9
endif
BUILD		:=	build
SOURCES		:=	source source/fatfs source/nand source/abstraction
DATA		:=	data
INCLUDES	:=	source source/fatfs source/nand

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork -flto

CFLAGS	:=	-g -Wall -Wextra -Wpedantic -pedantic -O2\
			-march=armv5te -mtune=arm946e-s -fomit-frame-pointer\
			-ffast-math -std=c99\
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DEXEC_$(EXEC_METHOD) -DARM9

CFLAGS	+=	-DBUILD_NAME="\"$(TARGET) (`date +'%Y/%m/%d'`)\""

ifneq ($(strip $(THEME)),)
CFLAGS	+=	-DUSE_THEME=\"\/$(THEME)\"
endif

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH) -DEXEC_$(EXEC_METHOD)
LDFLAGS	=	-nostartfiles -g $(ARCH) -Wl,-Map,$(TARGET).map

ifeq ($(EXEC_METHOD),GATEWAY)
	LDFLAGS += --specs=../gateway.specs
else ifeq ($(EXEC_METHOD),A9LH)
	LDFLAGS += --specs=../a9lh.specs
endif

ifeq ($(MODE),safe)
	CFLAGS += -DSAFEMODE
endif

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
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

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

export OFILES	:= $(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: common clean all gateway a9lh cakehax cakerop brahma release

#---------------------------------------------------------------------------------
all: a9lh

common:
	@[ -d $(OUTPUT_D) ] || mkdir -p $(OUTPUT_D)
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
    
submodules:
	@-git submodule update --init --recursive

gateway: common
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile EXEC_METHOD=GATEWAY
	@cp resources/LauncherTemplate.dat $(OUTPUT_D)/Launcher.dat
	@dd if=$(OUTPUT).bin of=$(OUTPUT_D)/Launcher.dat bs=1497296 seek=1 conv=notrunc

a9lh: common
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile EXEC_METHOD=A9LH

cakehax: submodules common
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile EXEC_METHOD=GATEWAY
	@make dir_out=$(OUTPUT_D) name=$(TARGET).dat -C CakeHax bigpayload
	@dd if=$(OUTPUT).bin of=$(OUTPUT).dat bs=512 seek=160
    
cakerop: cakehax
	@make DATNAME=$(TARGET).dat DISPNAME=$(TARGET) GRAPHICS=../resources/CakesROP -C CakesROP
	@mv CakesROP/CakesROP.nds $(OUTPUT_D)/$(TARGET).nds

brahma: submodules a9lh
	@[ -d BrahmaLoader/data ] || mkdir -p BrahmaLoader/data
	@cp $(OUTPUT).bin BrahmaLoader/data/payload.bin
	@cp resources/BrahmaAppInfo BrahmaLoader/resources/AppInfo
	@cp resources/BrahmaIcon.png BrahmaLoader/resources/icon.png
	@make --no-print-directory -C BrahmaLoader APP_TITLE=$(TARGET)
	@mv BrahmaLoader/output/*.3dsx $(OUTPUT_D)
	@mv BrahmaLoader/output/*.smdh $(OUTPUT_D)
	
release:
	@rm -fr $(BUILD) $(OUTPUT_D) $(RELEASE)
	@-make --no-print-directory cakerop
	@rm -fr $(BUILD) $(OUTPUT).bin $(OUTPUT).elf $(CURDIR)/$(LOADER)/data
	@make --no-print-directory brahma
	@[ -d $(RELEASE) ] || mkdir -p $(RELEASE)
	@[ -d $(RELEASE)/$(TARGET) ] || mkdir -p $(RELEASE)/$(TARGET)
	@-cp $(OUTPUT).bin $(RELEASE)
	@-cp $(OUTPUT).dat $(RELEASE)
	@-cp $(OUTPUT).nds $(RELEASE)
	@-cp $(OUTPUT).3dsx $(RELEASE)/$(TARGET)
	@-cp $(OUTPUT).smdh $(RELEASE)/$(TARGET)
	@cp $(CURDIR)/README.md $(RELEASE)
	@-7z a $(RELEASE)/$(TARGET)-`date +'%Y%m%d-%H%M%S'`.zip $(RELEASE)/*
	
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@-make clean --no-print-directory -C CakeHax
	@-make clean --no-print-directory -C CakesROP
	@-make clean --no-print-directory -C BrahmaLoader
	@rm -fr $(BUILD) $(OUTPUT_D) $(RELEASE)


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).bin	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)


#---------------------------------------------------------------------------------
%.bin: %.elf
	@$(OBJCOPY) --set-section-flags .bss=alloc,load,contents -O binary $< $@
	@echo built ... $(notdir $@)


-include $(DEPENDS)


#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
