#======================================================================
#	Makefile for idhcpc.x
#======================================================================

TARGET		= idhcpc.x
VERSION		= 011
ARCHIVE		= idhcp$(VERSION)
ARCFILES	= \
		Makefile \
		dhcp.c \
		main.c \
		nwsub.c \
		dhcp.h \
		nwsub.h \
		process.h \
		_keepchk.s \
		idhcpc.s \
		keepchk.s \
		tsrarea.s \
		history.txt \
		idhcpc.txt \
		$(TARGET)

CC	= gcc
CFLAGS	= -O2 -Wall
AS	= g2as
LDFLAGS	= -l
LDLIBS	= -lether -lnetwork
export G2LK	= -x

DHCP_H	= dhcp.h nwsub.h

.PHONY: all clean arc


all: $(TARGET)

$(TARGET): idhcpc.o tsrarea.o main.o dhcp.o nwsub.o keepchk.o _keepchk.o

main.o: nwsub.h $(DHCP_H)
dhcp.o: $(DHCP_H)
nwsub.o: nwsub.h


clean:
	-rm *.o

arc:
	-rm $(ARCHIVE).zip
	zip -9 $(ARCHIVE).zip $(ARCFILES)

