CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2 -I./include
TARGET  = ipc_simulator
SRCS    = src/ipc_core.c src/ipc_main.c
CORE    = src/ipc_core.c
TEST    = tests/test_ipc.c

.PHONY: all clean test run-all run-pipe run-fifo run-shm run-race run-bench

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

test: $(CORE) $(TEST)
	$(CC) $(CFLAGS) -o test_runner $^
	./test_runner

run-all: $(TARGET)
	./$(TARGET) 0

run-pipe: $(TARGET)
	./$(TARGET) 1

run-fifo: $(TARGET)
	./$(TARGET) 2

run-shm: $(TARGET)
	./$(TARGET) 3

run-race-unsafe: $(TARGET)
	./$(TARGET) 4

run-race-safe: $(TARGET)
	./$(TARGET) 5

run-bench: $(TARGET)
	./$(TARGET) 6

clean:
	rm -f $(TARGET) test_runner /tmp/ipc_sim_fifo
