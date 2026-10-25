CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDLIBS = -lgdbm -lm

# Homebrew on macOS keeps gdbm outside the default search paths
GDBM_PREFIX := $(shell brew --prefix gdbm 2>/dev/null)
ifneq ($(GDBM_PREFIX),)
CFLAGS  += -I$(GDBM_PREFIX)/include
LDFLAGS += -L$(GDBM_PREFIX)/lib
endif

BILLING_OBJS = main.o db.o billing.o server.o util.o

all: billing meter_agent

billing: $(BILLING_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(BILLING_OBJS) $(LDLIBS)

meter_agent: meter_agent.o
	$(CC) -o $@ meter_agent.o

%.o: %.c billing.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o billing meter_agent

.PHONY: all clean
