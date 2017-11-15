TARGET := $(shell basename $(CURDIR))

SOURCE := source
BUILD  := build

SUBARCH := -DARM11 -mcpu=mpcore -mtune=mpcore -mfloat-abi=soft -marm
INCDIRS := source
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
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD)/%.o: $(SOURCE)/%.c
	@mkdir -p "$(@D)"
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: $(SOURCE)/%.s
	@mkdir -p "$(@D)"
	$(CC) -c $(ASFLAGS) -o $@ $<
