CC = gcc
CFLAGS = -Wall -g
LFLAGS = -lpthread
DFLAGS =
PNAME =

OBJDIR = obj

MAINDEPS = flash_main.h
MAINSRC = flash_main.c

ifeq ($(mode), $(filter server_slave server_master, $(mode)))
	DFLAGS += -DNAME_SERVER
	DEPS = $(MAINSRC) $(wildcard server/*.h)
	SRC = $(MAINSRC) $(wildcard server/*.c)
else ifeq ($(mode), client)
	DFLAGS += -DCLIENT
	DEPS = $(MAINSRC) $(wildcard client/*.h)
	SRC = $(MAINSRC) $(wildcard client/*.c)
	PNAME = bin/client
endif

_OBJ = $(SRC:%.c=%.o)
OBJ = $(patsubst %,$(OBJDIR)/%,$(_OBJ))

ifeq ($(mode), server_master)
	DFLAGS += -DNAME_SERVER_MASTER
	PNAME = bin/server_master
else ifeq ($(mode), server_slave)
	DFLAGS += -DNAME_SERVER_SLAVE
	PNAME = bin/server_slave
endif

$(OBJDIR)/%.o: %.c $(DEPS)
	$(CC) $(DFLAGS) -c -o $@ $< $(CFLAGS)

default: $(OBJ)
	$(CC) $(DFLAGS) $^ -o $(PNAME) $(LFLAGS) $(CFLAGS)

clean:
	rm obj/*.o obj/server/* obj/client/*
