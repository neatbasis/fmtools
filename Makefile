# fmtools Makefile - just the basics for now

CC	= gcc
CFLAGS	= -Wall -O2

INSTALL	= /usr/bin/install

# Since fmtools doesn't use configure, these paths are provided here
# to make packaging easier.  Just override them when doing 'make install'.

BINPATH	= $(DESTDIR)/usr/local/bin
BINMODE = 0755
MANPATH = $(DESTDIR)/usr/local/man/man1
MANMODE = 0644

TARGETS	= fm fmscan

all: $(TARGETS)

fm: fm.c
	$(CC) $(CFLAGS) -o fm fm.c -lm

fmscan: fmscan.c
	$(CC) $(CFLAGS) -o fmscan fmscan.c -lm

clean: 
	rm -f *~ *.o $(TARGETS)

install: all install-bin install-man

install-bin:
	if (test ! -d $(BINPATH)); \
	then \
		mkdir -p $(BINPATH); \
	fi
	for f in $(TARGETS) ; do \
		$(INSTALL) -m $(BINMODE) $$f $(BINPATH); \
	done

install-man:
	if (test ! -d $(MANPATH)); \
	then \
		mkdir -p $(MANPATH); \
	fi
	for f in $(TARGETS) ; do \
		$(INSTALL) -m $(MANMODE) $$f.1 $(MANPATH); \
	done	

devices:
	mknod /dev/radio0 c 81 64
	ln -s /dev/radio0 /dev/radio
