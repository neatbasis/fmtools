# fmtools Makefile - just the basics for now

CC	= gcc
CFLAGS	= -Wall -O2

all : fm fmscan

fm : fm.c
	$(CC) $(CFLAGS) -o fm fm.c -lm

fmscan : fmscan.c
	$(CC) $(CFLAGS) -o fmscan fmscan.c -lm

clean : 
	rm -f *~ *.o fm fmscan

devices :
	mknod /dev/radio0 c 81 64
	ln -s /dev/radio0 /dev/radio
