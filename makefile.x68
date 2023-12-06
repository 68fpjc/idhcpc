EXECUTABLE = idhcpc.x

CC = gcc
CFLAGS = -O2 -Wall
AS = HAS060
LD = $(CC)
LDLIBS = -lether -lnetwork -ldos -liocs

export GCC_AS = $(AS)
export GCC_LINK = hlk
export GCC_LIB = .a
export GCC_NO__XCLIB = yes

.PHONY: all clean

all: $(EXECUTABLE)

OBJS = tsrarea.o keepchk.o __keepchk.o main.o idhcpc.o dhcp.o nwsub.o

$(EXECUTABLE): $(OBJS)
	$(LD) $(LDLIBS) $(OBJS) -o $(EXECUTABLE)

main.o: idhcpc.h
idhcpc.o: dhcp.h mynetwork.h nwsub.h
dhcp.o: mynetwork.h nwsub.h
nwsub.o: mynetwork.h

clean:
	-rm $(EXECUTABLE) $(OBJS)
