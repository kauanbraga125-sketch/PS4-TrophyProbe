TITLE       := PS4 TrophyProbe
VERSION     := 0.10
TITLE_ID    := BREWTP001
CONTENT_ID  := IV0000-BREWTP001_00-PS4TROPHYPROBE01

LIBS        := -lc -lkernel -lc++ -lSceUserService -lSceNpTrophy -lSceSysmodule

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
PROJDIR     := source
INTDIR      := build

CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
OBJS        := $(patsubst $(PROJDIR)/%.cpp,$(INTDIR)/%.o,$(CPPFILES))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
CCX  := clang++
LD   := ld.lld
CDIR := linux
endif
ifeq ($(UNAME_S),Darwin)
CCX  := /usr/local/opt/llvm/bin/clang++
LD   := /usr/local/opt/llvm/bin/ld.lld
CDIR := macos
endif

all: eboot.bin

$(INTDIR):
	mkdir -p $(INTDIR)

$(INTDIR)/%.o: $(PROJDIR)/%.cpp | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/trophyprobe.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/trophyprobe.elf -out=$(INTDIR)/trophyprobe.oelf --eboot "eboot.bin" --paid 0x3800000000000011

clean:
	rm -rf $(INTDIR) eboot.bin

.PHONY: all clean
