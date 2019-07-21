PROCESSOR := ARM9

TARGET := $(shell basename $(CURDIR))

SOURCE := source
BUILD  := build

SUBARCH := -D$(PROCESSOR) -marm -march=armv5te -mtune=arm946e-s -mfloat-abi=soft -mno-thumb-interwork
INCDIRS := source source/common source/filesys source/crypto source/fatfs source/nand source/virtual source/game source/gamecart source/lodepng source/qrcodegen source/system source/utils
INCLUDE := $(foreach dir,$(INCDIRS),-I"$(shell pwd)/$(dir)")

ASFLAGS += $(SUBARCH) $(INCLUDE)
CFLAGS  += $(SUBARCH) $(INCLUDE) -fno-builtin-memcpy -flto
LDFLAGS += $(SUBARCH) -Wl,-Map,$(TARGET).map -flto

include ../Makefile.common
include ../Makefile.build
