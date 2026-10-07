# GCC cross-build for AmigaOS 3, with generated register entry gates.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SECONDARY:
CROSS ?= m68k-amigaos-
CC := $(CROSS)gcc
STRIP := $(CROSS)strip
HOSTCC ?= cc
PYTHON ?= python3
LHA ?= lha
RELEASE_DIR ?= $(BUILD)/release
SFDC ?= sfdc
FD2SFD ?= fd2sfd
BUILD ?= ../../../build/muimaster
CPUFLAGS ?= -m68000 -msoft-float
TRACE ?= 0
ifneq ($(TRACE),0)
ifneq ($(TRACE),1)
$(error TRACE must be 0 or 1)
endif
endif
CRTFLAGS = -noixemul
CFLAGS ?= -std=gnu17 -Os -g -Wall -Wno-pointer-sign
CPPFLAGS += -DMUIMASTER_YES_INLINE_STDARG= -DZUNE68_GCC_NATIVE \
  -DMUI_OBSOLETE -DMUIMASTER_LIBRARY_BUILD -DZUNE_NO_CYBERGRAPHICS \
  -I$(BUILD)/include -I$(BUILD) -Igcc -Iinclude -I. -include include/clib/muimaster_protos.h \
  -include $(BUILD)/trace-config.h
LIB_CFLAGS = $(CPUFLAGS) $(CRTFLAGS) $(CFLAGS) \
  -fno-common -fno-builtin -fno-strict-aliasing
SOURCES = \
  gcc/format.c gcc/runtime.c muimaster_init.c datatypescache.c dragndrop.c font.c \
  frame.c imspec.c imspec_vector.c imspec_gradient.c \
  imspec_gradientdraw.c menu.c penspec.c support.c support_amigaos.c \
  support_classes.c textengine.c bubbleengine.c clipboard.c locale.c \
  mui_addclipping.c mui_addclipregion.c mui_allocaslrequest.c \
  staticlib/mui_newobject.c staticlib/mui_makeobject.c staticlib/mui_request.c staticlib/mui_allocaslrequesttags.c mui_aslrequest.c \
  staticlib/mui_aslrequesttags.c mui_beginrefresh.c \
  mui_createcustomclass.c mui_deletecustomclass.c mui_disposeobject.c \
  mui_endrefresh.c mui_error.c mui_freeaslrequest.c mui_freeclass.c \
  mui_getclass.c mui_layout.c mui_makeobject.c mui_newobject.c \
  mui_obtainpen.c mui_redraw.c mui_rejectidcmp.c \
  mui_releasepen.c mui_removeclipping.c mui_removeclipregion.c \
  mui_request.c mui_requestidcmp.c mui_seterror.c classes/aboutmui.c \
  classes/application.c classes/area.c classes/balance.c classes/bitmap.c \
  classes/bodychunk.c classes/boopsi.c classes/chunkyimage.c \
  classes/configdata.c classes/coloradjust.c classes/colorfield.c \
  classes/crawling.c classes/cycle.c classes/dataspace.c \
  classes/dirlist.c classes/dtpic.c classes/family.c classes/floattext.c \
  classes/frameadjust.c classes/framedisplay.c classes/gauge.c \
  classes/group.c classes/image.c classes/imageadjust.c \
  classes/imagedisplay.c classes/knob.c classes/levelmeter.c \
  classes/list.c classes/listview.c classes/mccprefs.c classes/menuitem.c \
  classes/notify.c classes/numeric.c classes/numericbutton.c \
  classes/palette.c classes/penadjust.c classes/pendisplay.c \
  classes/poppen.c classes/popasl.c classes/popframe.c classes/popimage.c \
  classes/poplist.c classes/popobject.c classes/popscreen.c \
  classes/popstring.c classes/prop.c classes/radio.c classes/rectangle.c \
  classes/register.c classes/semaphore.c classes/scale.c \
  classes/scrollbar.c classes/scrollbutton.c classes/scrollgroup.c \
  classes/settings.c classes/settingsgroup.c classes/slider.c \
  classes/string.c classes/text.c classes/virtgroup.c \
  classes/volumelist.c classes/window.c
OBJECTS = $(addprefix $(BUILD)/,$(SOURCES:.c=.o))
LDLIBS = -Wl,--start-group -lgcc -lnix20 -lnix -lm -lamiga -ldebug \
  -Wl,--end-group
GENERATED = $(BUILD)/muimaster_strings.h $(BUILD)/trace-config.h $(BUILD)/include/libraries/mui.h \
  $(BUILD)/include/inline/muimaster.h $(BUILD)/include/inline/muiscreen.h $(BUILD)/gates.h

.PHONY: all clean check check-core check-sdk release FORCE
all: core $(BUILD)/compatcheck
core: $(BUILD)/muimaster.library $(BUILD)/zunemaster.library $(BUILD)/opentest

release: all
	$(PYTHON) ../../../tools/release.py --build-dir "$(BUILD)" \
	  --output-dir "$(RELEASE_DIR)" --lha "$(LHA)" --strip "$(STRIP)"

# Keep the original 3.8 invocation while allowing a separate 3.9 SDK.
MUI_SDK_VERSION ?= 3.8
MUI_SDK ?= $(MUI38_SDK)
MUI_SDK_LABEL = mui$(subst .,,$(MUI_SDK_VERSION))
check-sdk: $(BUILD)/compatcheck
	@test -n "$(MUI_SDK)" || { echo 'Set MUI_SDK to the original SDK directory'; exit 1; }
	$(PYTHON) ../../../tools/check_sdk.py --sdk "$(MUI_SDK)" --release "$(MUI_SDK_VERSION)" \
	  --output "$(BUILD)/sdk-check" --cc "$(CC)" --sfdc "$(SFDC)" --fd2sfd "$(FD2SFD)"
	$(PYTHON) ../../../tests/check_opentest_stack.py $(BUILD)/compatcheck $(BUILD)/sdk-check/compatcheck-$(MUI_SDK_LABEL)

$(BUILD)/compatcheck: ../../../tests/native/compatcheck.c $(GENERATED)
	$(CC) $(CPUFLAGS) $(CRTFLAGS) $(CFLAGS) -DMUIMASTER_YES_INLINE_STDARG= \
	  -I$(BUILD)/include -Iinclude -Wl,-u,___stkinit -o $@ $< -lmui

check: all check-core

check-core: core $(BUILD)/compatcheck
	$(PYTHON) ../../../tests/check_native_init.py $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_gcc_library.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_class_lifetime.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_window_disposal.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_floattext.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_notifications.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_application_services.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_dynamic_layout.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_resources.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_clipping.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_list_sort.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_trace.py \
	  $(BUILD)/zunemaster.library $(BUILD)/muimaster.library
	$(PYTHON) ../../../tests/check_release.py
	$(PYTHON) ../../../tests/check_opentest_stack.py $(BUILD)/opentest $(BUILD)/compatcheck

# Changing TRACE in an existing build must also recompile the callers.
$(BUILD)/trace-config.h: FORCE
	@mkdir -p $(@D)
	@printf '#define ZUNE68_TRACE %s\n' '$(TRACE)' | cmp -s - $@ || \
	  printf '#define ZUNE68_TRACE %s\n' '$(TRACE)' > $@

FORCE:

$(BUILD)/buildincludes: buildincludes.c GNUmakefile
	@mkdir -p $(@D)
	$(HOSTCC) -O2 -Wall -Wno-pointer-sign -o $@ $<

$(BUILD)/include/libraries/mui.h: $(BUILD)/buildincludes $(wildcard *.h classes/*.h)
	@mkdir -p $(@D)
	$(BUILD)/buildincludes > $@.tmp
	mv $@.tmp $@

# Old SFD typedef hints are names, not complete C typedef declarations.
$(BUILD)/muimaster.sfd: zunemaster_lib.sfd
	@mkdir -p $(@D)
	sed '/^==typedef /d' $< > $@

$(BUILD)/include/inline/muimaster.h: $(BUILD)/muimaster.sfd
	@mkdir -p $(@D)
	$(SFDC) --quiet --target=m68k-amigaos --mode=macros --output=$@ $<

$(BUILD)/muiscreen.sfd: include/fd/muiscreen_lib.fd include/clib/muiscreen_protos.h
	@mkdir -p $(@D)
	$(FD2SFD) --quiet $^ $@

$(BUILD)/include/inline/muiscreen.h: $(BUILD)/muiscreen.sfd
	@mkdir -p $(@D)
	$(SFDC) --quiet --target=m68k-amigaos --mode=macros --output=$@ $<

$(BUILD)/gates.c: $(BUILD)/muimaster.sfd
	$(SFDC) --quiet --target=m68k-amigaos --mode=gatestubs --gateprefix=Gate_ --output=$@ $<

$(BUILD)/gates.h: $(BUILD)/muimaster.sfd
	$(SFDC) --quiet --target=m68k-amigaos --mode=gateproto --gateprefix=Gate_ --output=$@ $<

$(BUILD)/gates.o: $(BUILD)/gates.c $(GENERATED)
	$(CC) $(CPPFLAGS) $(LIB_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.c $(GENERATED)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(LIB_CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/start.o: gcc/start.S
	@mkdir -p $(@D)
	$(CC) $(CPUFLAGS) -c $< -o $@

$(BUILD)/muimaster_lib.o: zunemaster_lib.c $(GENERATED)
	$(CC) $(CPPFLAGS) $(LIB_CFLAGS) -I$(BUILD) -DMUIMASTER_DROPIN -MMD -MP -c $< -o $@

$(BUILD)/zunemaster_lib.o: zunemaster_lib.c $(GENERATED)
	$(CC) $(CPPFLAGS) $(LIB_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@

# Keep start.o first regardless of the compiler's function ordering.
$(BUILD)/%.library: $(BUILD)/start.o $(BUILD)/%_lib.o $(BUILD)/gates.o $(OBJECTS)
	$(CC) $(CPUFLAGS) $(CRTFLAGS) -nostdlib $(LDFLAGS) \
	  -Wl,-Map,$@.map -o $@ $^ $(LDLIBS)

$(BUILD)/opentest: opentest.c $(GENERATED)
	$(CC) $(CPUFLAGS) $(CRTFLAGS) $(CFLAGS) -DMUIMASTER_YES_INLINE_STDARG= -I$(BUILD)/include -Iinclude -Wl,-u,___stkinit -o $@ $<

$(OBJECTS) $(BUILD)/gates.o $(BUILD)/muimaster_lib.o \
  $(BUILD)/zunemaster_lib.o $(BUILD)/opentest: GNUmakefile

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d) $(BUILD)/gates.d \
  $(BUILD)/muimaster_lib.d $(BUILD)/zunemaster_lib.d

include ../../../tools/components.mk

$(BUILD)/muimaster_strings.h: catalogs/muimaster.cd
	@mkdir -p $(@D)
	$(FLEXCAT) $< $@=../../../tools/flexcat/CatComp_h.sd
