# Simple Makefile for compiling X11/math C programs

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11 -lm

# Directories
SRCDIR = .
BINDIR = bin

# Find all .c files in the source directory
SOURCES = $(wildcard $(SRCDIR)/*.c)

# Create a list of executable names from the source file names
# e.g., src/program.c becomes bin/program
EXECUTABLES = $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%,$(SOURCES))

# Default target: build all executables
all: $(EXECUTABLES)

# Rule to build an executable from a .c file
# $< is the first prerequisite (the .c file)
# $@ is the target (the executable)
$(BINDIR)/%: $(SRCDIR)/%.c
	@echo "Compiling $< -> $@"
	@$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Target to clean up the build artifacts
clean:
	@echo "Cleaning up..."
	@rm -f $(BINDIR)/*

# Phony targets are not actual files
.PHONY: all clean
