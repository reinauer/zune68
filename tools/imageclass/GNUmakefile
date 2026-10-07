.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
ROOT := $(abspath ../..)
BUILD ?= $(ROOT)/build/muimaster
CROSS ?= m68k-amigaos-
CC := $(CROSS)gcc
CPUFLAGS ?= -m68000 -msoft-float
CFLAGS ?= -std=gnu17 -Os -g -Wall -Wno-pointer-sign
MASTER := $(ROOT)/workbench/libs/muimaster
RAW := $(ROOT)/workbench/classes/zune/rawimage
COMMON := $(ROOT)/workbench/classes/zune/nlist/include
BZ := $(ROOT)/vendor/bzip2
OBJ := $(BUILD)/imageclass
FLAGS := $(CPUFLAGS) -noixemul $(CFLAGS) -fno-builtin -fno-common \
 -fno-strict-aliasing -D__amigaos3__ -DZUNE68_GCC_NATIVE \
 -DNO_INLINE_STDARG -U__saveds -D__saveds= \
 -I$(BUILD)/include -I$(MASTER)/include -I$(RAW) -iquote $(MASTER)/classes \
 -I$(ROOT)/workbench/prefs/Zune/include -I$(COMMON) -I$(BZ)
BZFILES := bzlib decompress huffman crctable randtable blocksort compress
BZOBJS := $(addprefix $(OBJ)/bz2/,$(addsuffix .o,$(BZFILES)))
.PHONY: all
all: $(BUILD)/Libs/MUI/Rawimage.mcc $(BUILD)/Libs/MUI/Pixmap.mui
$(OBJ)/rawimage.o: library.c $(RAW)/Rawimage.c $(RAW)/Rawimage_private.h GNUmakefile
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -DBUILD_RAWIMAGE -MMD -MP -c $< -o $@
$(OBJ)/pixmap.o: library.c $(MASTER)/classes/pixmap.c $(MASTER)/classes/pixmap_private.h GNUmakefile
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
$(OBJ)/bz2/%.o: $(BZ)/%.c $(BZ)/bzlib.h $(BZ)/bzlib_private.h GNUmakefile
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -DBZ_NO_STDIO -Wno-unused-but-set-variable -c $< -o $@
$(OBJ)/superattrs.o: ../plugin/superattrs.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -c $< -o $@
$(BUILD)/Libs/MUI/Rawimage.mcc: $(OBJ)/rawimage.o $(OBJ)/superattrs.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(BUILD)/Libs/MUI/Pixmap.mui: $(OBJ)/pixmap.o $(OBJ)/superattrs.o $(BZOBJS)
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
-include $(OBJ)/rawimage.d $(OBJ)/pixmap.d
