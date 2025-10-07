CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2 -g
LDFLAGS =
INCLUDES = -I.

SRCS = main.c member_node_procedure.c master_node_procedure.c sock_wrapper_functions.c
OBJS = $(SRCS:.c=.o)
TARGET = nodeinfo_collector_nattrav.bin

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)