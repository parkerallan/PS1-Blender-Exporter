TARGET = modelviewer
TYPE = ps-exe
BINDIR = build/

SRCS = main.c \
lib/model.c \
lib/display.c \
lib/lighting.c \
lib/input.c \
lib/texture.c \
lib/camera.c \
lib/animation.c \
lib/sound.c \
../../third_party/nugget/common/crt0/crt0.s

CPPFLAGS += -I../../third_party/psyq/include
CPPFLAGS += -I.
LDFLAGS += -L../../third_party/psyq/lib
LDFLAGS += -Wl,--start-group
LDFLAGS += -lapi
LDFLAGS += -lc
LDFLAGS += -lc2
LDFLAGS += -lcd
LDFLAGS += -lcard
LDFLAGS += -lcomb
LDFLAGS += -lds
LDFLAGS += -letc
LDFLAGS += -lgpu
LDFLAGS += -lgs
LDFLAGS += -lgte
LDFLAGS += -lgun
LDFLAGS += -lhmd
LDFLAGS += -lmath
LDFLAGS += -lmcrd
LDFLAGS += -lmcx
LDFLAGS += -lpad
LDFLAGS += -lpress
LDFLAGS += -lsio
LDFLAGS += -lsnd
LDFLAGS += -lspu
LDFLAGS += -ltap
LDFLAGS += -Wl,--end-group

# Tell make where to find source files
VPATH = .:lib

include ../../third_party/nugget/common.mk

# Also clean .dep files from build directory
clean::
	rm -f build/*.dep
