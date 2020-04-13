include ../../Common.mk

# NOTE: This Makefile was made by hand, not via the outdated autoconf
# in this same folder.

all: librsynth$(ALIB_EXT)

C_FILES:=$(wildcard src/*.c)
O_FILES:=$(patsubst %.c,%$(OBJ_EXT),$(C_FILES))

CFLAGS = $(COMMON_CFLAGS) -I. -Iinclude -Iinclude/rsynth

$(O_FILES): %$(OBJ_EXT): %.c
	$(CC) $(CFLAGS) $(NOLINK_OPT) $< $(OBJ_OUT_OPT)$@

librsynth$(ALIB_EXT): $(O_FILES)
	$(AR) $(AR_OPTS) $(AR_OUT_OPT)$@ $^

.PHONY: clean
clean:
	rm -f src/*.o *.a *.lib *.obj *.o-arm* *.o-sim*
