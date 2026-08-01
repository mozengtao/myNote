# Generic Timer Library Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = 

# Source files
TIMER_SRCS = timer.c
DEMO_SRCS = main.c
HEADERS = timer.h

# Object files
TIMER_OBJS = $(TIMER_SRCS:.c=.o)
DEMO_OBJS = $(DEMO_SRCS:.c=.o)

# Targets
TARGET = timer_demo
LIBRARY = libtimer.a

.PHONY: all clean demo library test

# Default target
all: demo

# Build demo program
demo: $(TARGET)

# Build static library
library: $(LIBRARY)

$(TARGET): $(TIMER_OBJS) $(DEMO_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Demo program built successfully: $(TARGET)"

$(LIBRARY): $(TIMER_OBJS)
	ar rcs $@ $^
	@echo "Static library built successfully: $(LIBRARY)"

# Compile source files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Test compilation only
test:
	@echo "Testing compilation..."
	$(CC) $(CFLAGS) -c timer.c -o timer_test.o
	$(CC) $(CFLAGS) -c main.c -o main_test.o
	$(CC) $(LDFLAGS) timer_test.o main_test.o -o test_program
	@echo "Compilation test successful"
	@rm -f timer_test.o main_test.o test_program

# Clean up
clean:
	rm -f *.o $(TARGET) $(LIBRARY) test_program
	@echo "Clean completed"

# Install (copy headers and library to system paths)
install: $(LIBRARY)
	@echo "Install target not implemented yet"

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build demo program (default)"
	@echo "  demo     - Build demo program"
	@echo "  library  - Build static library"
	@echo "  test     - Test compilation only"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help message"