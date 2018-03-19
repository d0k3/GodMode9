PROCESSOR := ARM9

TARGET := $(shell basename $(CURDIR))

SOURCE := source
BUILD  := build

SUBARCH := -D$(PROCESSOR) -mcpu=arm946e-s -mtune=arm946e-s -mfloat-abi=soft -mthumb
INCDIRS := source source/common source/filesys source/crypto source/fatfs source/nand source/virtual source/game source/gamecart source/quicklz source/qrcodegen source/system source/utils
INCLUDE := $(foreach dir,$(INCDIRS),-I"$(shell pwd)/$(dir)")

ASFLAGS += $(SUBARCH) $(INCLUDE)
CFLAGS  += $(SUBARCH) $(INCLUDE)
LDFLAGS += $(SUBARCH) -Wl,-Map,$(TARGET).map

include ../Makefile.common

OBJECTS = $(patsubst $(SOURCE)/%.s, $(BUILD)/%.o, \
          $(patsubst $(SOURCE)/%.c, $(BUILD)/%.o, \
          $(call rwildcard, $(SOURCE), *.s *.c)))

.PHONY: all
all: $(TARGET).elf

.PHONY: clean
clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).map

$(TARGET).elf: $(OBJECTS)
	@mkdir -p "$(@D)"
	@echo "[$(PROCESSOR)] $<"
	@$(CC) $(LDFLAGS) $^ -o $@

$(BUILD)/%.o: $(SOURCE)/%.c
	@mkdir -p "$(@D)"
	@echo "[$(PROCESSOR)] $<"
	@$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: $(SOURCE)/%.s
	@mkdir -p "$(@D)"
	@echo "[$(PROCESSOR)] $<"
	@$(CC) -c $(ASFLAGS) -o $@ $<
