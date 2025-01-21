EXECUTABLE = idhcpc.x

CC = gcc
CFLAGS = -O2 -Wall
AS = HAS060
LD = $(CC)
LDLIBS = -lnetwork -ldos -lgcc

export GCC_AS = $(AS)
export GCC_LINK = hlk
export GCC_LIB = .a
export GCC_NO__XCLIB = yes

.PHONY: all clean deps

all: $(EXECUTABLE)

OBJS_C = tsrarea.o idhcpc.o dhcp.o nwsub.o main.o
OBJS_S = keepchk.o __keepchk.o

$(EXECUTABLE): $(OBJS_C) $(OBJS_S)
	$(LD) $(LDLIBS) $^ -o $(EXECUTABLE)

tsrarea.o : tsrarea.c idhcpc.h
idhcpc.o : idhcpc.c idhcpc.h dhcp.h mynetwork.h nwsub.h
dhcp.o : dhcp.c dhcp.h mynetwork.h
nwsub.o : nwsub.c nwsub.h mynetwork.h
main.o : main.c idhcpc.h

tsrarea.o: tsrarea.c
	$(CC) $(CFLAGS) -c -o $@ $< -fall-text

clean:
	-rm $(EXECUTABLE) $(OBJS_C) $(OBJS_S)

deps: $(patsubst %.o,%.c,$(OBJS_C))
	@$(CC) -MM $^
