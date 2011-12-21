CFLAGS= -g -pedantic -Wall -std=c99
CFILES=$(shell ls | grep .c)
OBJS=$(CFILES:%.c=%.o)

all: mgrep

clean:
	$(RM) mgrep $(OBJS)

mgrep: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@