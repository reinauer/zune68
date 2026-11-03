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
LISTTREE := $(ROOT)/workbench/classes/zune/listtree
BUSY := $(ROOT)/workbench/classes/zune/busy
CALLTIPS := $(ROOT)/workbench/classes/zune/calltips
COMMON := $(ROOT)/workbench/classes/zune/nlist/include
BZ := $(ROOT)/vendor/bzip2
OBJ := $(BUILD)/imageclass
FLAGS := $(CPUFLAGS) -noixemul $(CFLAGS) -fno-builtin -fno-common \
 -fno-strict-aliasing -D__amigaos3__ -DZUNE68_GCC_NATIVE -DNDEBUG \
 -DNO_INLINE_STDARG -U__saveds -D__saveds= \
 -DMUIMASTER_NAME='"muimaster.library"' \
 -I$(BUILD)/include -I$(MASTER)/include -I$(RAW) -iquote $(MASTER)/classes \
 -I$(ROOT)/workbench/prefs/Zune/include -I$(COMMON) -I$(BZ)
BZFILES := bzlib decompress huffman crctable randtable blocksort compress
BZOBJS := $(addprefix $(OBJ)/bz2/,$(addsuffix .o,$(BZFILES)))
.PHONY: all
all: $(BUILD)/Libs/MUI/Rawimage.mcc $(BUILD)/Libs/MUI/Pixmap.mui \
 $(BUILD)/Libs/MUI/Listtree.mcc \
 $(BUILD)/Libs/MUI/Busy.mcc \
 $(BUILD)/Libs/MUI/Calltips.mcc \
 $(BUILD)/Libs/MUI/Textinput.mcc $(BUILD)/Libs/MUI/Textinputscroll.mcc
$(OBJ)/rawimage.o: library.c $(RAW)/Rawimage.c $(RAW)/Rawimage_private.h
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -DBUILD_RAWIMAGE -MMD -MP -c $< -o $@
$(OBJ)/pixmap.o: library.c $(MASTER)/classes/pixmap.c $(MASTER)/classes/pixmap_private.h
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
$(OBJ)/listtree.o: listtree_library.c $(wildcard $(LISTTREE)/*.[ch])
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -Wno-incompatible-pointer-types -I. -I$(LISTTREE) -MMD -MP -c $< -o $@
$(OBJ)/bz2/%.o: $(BZ)/%.c $(BZ)/bzlib.h $(BZ)/bzlib_private.h
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -DBZ_NO_STDIO -Wno-unused-but-set-variable -MMD -MP -c $< -o $@
$(OBJ)/superattrs.o: ../plugin/superattrs.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/Libs/MUI/Rawimage.mcc: $(OBJ)/rawimage.o $(OBJ)/superattrs.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(BUILD)/Libs/MUI/Pixmap.mui: $(OBJ)/pixmap.o $(OBJ)/superattrs.o $(BZOBJS)
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(OBJ)/busy.o: busy_library.c $(wildcard $(BUSY)/*.[ch])
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -I. -I$(BUSY) -MMD -MP -c $< -o $@
$(BUILD)/Libs/MUI/Busy.mcc: $(OBJ)/busy.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(OBJ)/calltips.o: calltips_library.c $(wildcard $(CALLTIPS)/*.[ch])
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -I. -I$(CALLTIPS) -MMD -MP -c $< -o $@
$(BUILD)/Libs/MUI/Calltips.mcc: $(OBJ)/calltips.o $(OBJ)/superattrs.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(OBJ)/textinput.o: textinput_library.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -I. -MMD -MP -c $< -o $@
$(OBJ)/textinputscroll.o: textinput_library.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -I. -DTEXTINPUTSCROLL -MMD -MP -c $< -o $@
$(BUILD)/Libs/MUI/Textinput.mcc: $(OBJ)/textinput.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(BUILD)/Libs/MUI/Textinputscroll.mcc: $(OBJ)/textinputscroll.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
$(OBJ)/format.o: ../plugin/format.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/Libs/MUI/Listtree.mcc: $(OBJ)/listtree.o $(OBJ)/format.o
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $@ $^ -Wl,--start-group -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
# AROS classes whose library envelope is generated from their .conf file.
ZUNE := $(ROOT)/workbench/classes/zune
CLASS_NAMES := Aboutbox AboutWindow Calendar Clock Flexgroup Graph IconImage \
 Lamp LoginWindow PrefsEditor PrefsWindow QRCode SystemPrefsWindow
CLASS_BINS := $(CLASS_NAMES:%=$(BUILD)/Libs/MUI/%.mcc)
CLASS_HEADERS := iconimage prefseditor prefswindow qrcode
CLASS_SUPPORT := $(OBJ)/class_support.o $(OBJ)/superattrs.o $(OBJ)/strings.o $(OBJ)/format.o
GEN := $(OBJ)/generated
FLEXCAT ?= flexcat
PYTHON ?= python3
all: $(CLASS_BINS)
$(GEN)/include/zune/%.h: $(ZUNE)/iconimage/%.h
	@mkdir -p $(@D)
	cp $< $@
$(GEN)/include/zune/%.h: $(ZUNE)/prefseditor/%.h
	@mkdir -p $(@D)
	cp $< $@
$(GEN)/include/zune/%.h: $(ZUNE)/prefswindow/%.h
	@mkdir -p $(@D)
	cp $< $@
$(GEN)/include/zune/%.h: $(ZUNE)/qrcode/%.h
	@mkdir -p $(@D)
	cp $< $@
$(GEN)/aboutwindow/strings.h: $(ZUNE)/aboutwindow/catalogs/aboutwindow.cd
	@mkdir -p $(@D)
	$(FLEXCAT) $< $@=$(ROOT)/tools/flexcat/CatComp_h.sd
$(GEN)/systemprefswindow/strings.h: $(ZUNE)/systemprefswindow/catalogs/systemprefswindow.cd
	@mkdir -p $(@D)
	$(FLEXCAT) $< $@=$(ROOT)/tools/flexcat/CatComp_h.sd
$(GEN)/prefswindow/prefswindow_strings.h: $(ZUNE)/prefswindow/catalogs/prefswindow.cd
	@mkdir -p $(@D)
	$(FLEXCAT) $< $@=$(ROOT)/tools/flexcat/CatComp_h.sd
$(OBJ)/class_support.o: class_support.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
$(OBJ)/strings.o: ../plugin/strings.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@
define CLASS_RULE
$(OBJ)/$(1).o: $(wildcard $(ZUNE)/$(2)/*.[ch]) $(wildcard $(ZUNE)/$(2)/*.conf) \
 gen_class.py class_common.h class_libdefs.h \
 $(CLASS_HEADERS:%=$(GEN)/include/zune/%.h) $(3)
	@mkdir -p $(GEN)/$(2)
	$(PYTHON) gen_class.py $(firstword $(wildcard $(ZUNE)/$(2)/*.conf)) $(GEN)/$(2)/$(2)_library.c
	$(CC) -iquote $(GEN)/$(2) $(FLAGS) $(4) -I. -I$(GEN)/include -I$(ZUNE)/$(2) \
	 -MMD -MP -c $(GEN)/$(2)/$(2)_library.c -o $$@
$(BUILD)/Libs/MUI/$(1).mcc: $(OBJ)/$(1).o $(CLASS_SUPPORT)
	@mkdir -p $$(@D)
	$(CC) $(CPUFLAGS) -noixemul -nostdlib -o $$@ $$^ -Wl,--start-group -lm -lmui -lgcc -lnix20 -lnix -lamiga -ldebug -Wl,--end-group
endef
$(eval $(call CLASS_RULE,Aboutbox,aboutbox,,-UNO_INLINE_STDARG))
$(eval $(call CLASS_RULE,AboutWindow,aboutwindow,$(GEN)/aboutwindow/strings.h,-Daboutwindow_ARRAY -DCatCompArray=aboutwindow_Array))
$(eval $(call CLASS_RULE,Calendar,calendar))
$(eval $(call CLASS_RULE,Clock,clock))
$(eval $(call CLASS_RULE,Flexgroup,flexgroup))
$(eval $(call CLASS_RULE,Graph,graph))
$(eval $(call CLASS_RULE,IconImage,iconimage))
$(eval $(call CLASS_RULE,Lamp,lamp))
$(eval $(call CLASS_RULE,LoginWindow,loginwindow,,-Wno-int-conversion))
$(eval $(call CLASS_RULE,PrefsEditor,prefseditor))
$(eval $(call CLASS_RULE,PrefsWindow,prefswindow,$(GEN)/prefswindow/prefswindow_strings.h))
$(eval $(call CLASS_RULE,QRCode,qrcode))
$(eval $(call CLASS_RULE,SystemPrefsWindow,systemprefswindow,$(GEN)/systemprefswindow/strings.h,-Dsystemprefswindow_ARRAY -DCatCompArray=systemprefswindow_Array))
OBJECTS := $(addprefix $(OBJ)/,rawimage.o pixmap.o listtree.o busy.o calltips.o \
 textinput.o textinputscroll.o $(CLASS_NAMES:=.o)) $(BZOBJS) $(CLASS_SUPPORT)
include $(ROOT)/tools/flags.mk
$(OBJ)/cflags: FORCE
	@$(call record_flags,$(CC) $(FLAGS))
$(OBJECTS): GNUmakefile $(OBJ)/cflags
-include $(OBJECTS:.o=.d)
