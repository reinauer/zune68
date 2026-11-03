# A flags file holds the compiler command line of the objects that depend
# on it and is rewritten only when that line changes, so changing CFLAGS,
# CPPFLAGS, TRACE or LTO in an existing build directory recompiles them.
# Use as the recipe of a target that depends on FORCE:
#   $(BUILD)/cflags: FORCE
#   	@$(call record_flags,$(CC) $(CFLAGS))
.PHONY: FORCE
FORCE:
record_flags = mkdir -p $(@D); printf '%s\n' '$(subst ','\'',$(1))' | cmp -s - $@ || \
  printf '%s\n' '$(subst ','\'',$(1))' > $@
