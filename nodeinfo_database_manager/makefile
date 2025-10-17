CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -O2 -g
LDFLAGS :=

TARGET := nodeinfo_database_manager.bin
SRCS := main.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean run rebuild

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all
