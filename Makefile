CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -O2

# Don't make pedantic checks errors,
# as vanilla libusb-1.0.8 can't live with that
#CFLAGS += -pedantic-errors

# GCC >= 4.6
#CFLAGS += -Wunused-but-set-variable

CFLAGS += -fno-common \
  -Wall \
  -Wextra \
  -Wformat=2 \
  -Winit-self \
  -Winline \
  -Wpacked \
  -Wp,-D_FORTIFY_SOURCE=2 \
  -Wpointer-arith \
  -Wlarger-than-65500 \
  -Wmissing-declarations \
  -Wmissing-format-attribute \
  -Wmissing-noreturn \
  -Wmissing-prototypes \
  -Wnested-externs \
  -Wold-style-definition \
  -Wredundant-decls \
  -Wsign-compare \
  -Wstrict-aliasing=2 \
  -Wstrict-prototypes \
  -Wswitch-enum \
  -Wundef \
  -Wunreachable-code \
  -Wunsafe-loop-optimizations \
  -Wwrite-strings

CFLAGS  += $(shell pkg-config --cflags libusb-1.0)
LDLIBS += $(shell pkg-config --libs libusb-1.0)

PREFIX ?= /usr/local
bindir := $(PREFIX)/sbin

all: picoproj

CFLAGS += -D_BSD_SOURCE # for htole32()
CFLAGS += -D_POSIX_C_SOURCE=2 # for getopt()

picoproj: picoproj.o am7xxx.o

install: picoproj
	install -d $(DESTDIR)$(bindir)
	install -m 755 picoproj $(DESTDIR)$(bindir)

BACKUP_PREFIX=libpicoproj-$(shell date +%Y%m%d%H%M)
backup:
	git archive \
	  -o $(BACKUP_PREFIX).tar.gz \
	  --prefix=$(BACKUP_PREFIX)/ \
	  HEAD

changelog:
	git log --pretty="format:%ai  %aN  <%aE>%n%n%x09* %s%d%n" > ChangeLog

clean:
	rm -rf *~ *.o picoproj
