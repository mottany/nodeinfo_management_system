CC := gcc
CFLAGS := -std=gnu11 -Wall -Wextra -O2 -g
LDFLAGS :=

TARGET := nodeinfo_database_manager.bin
SRCS := main.c ctrl_service.c nodedata_receiver.c db_manager.c
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

# --- tests ---
.PHONY: test
test: test_db_manager

test_db_manager: db_manager.o
	$(CC) $(CFLAGS) test_db_manager.c db_manager.o -o test_db_manager.bin $(LDFLAGS)

test-run: test_db_manager
	./test_db_manager.bin
