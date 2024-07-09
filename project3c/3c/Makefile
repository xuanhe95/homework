# Define variable CC to be the compiler used in this project
CC=gcc

# Define CFLAGS for the flags used along with the compiler
CFLAGS = -g -Wall

# Define TARGETS to be the targets to be run when calling 'make all'
TARGETS = clean hellomake

# Define PHONY targets to prevent make from confusing the phony target with the same file names
.PHONY: clean all

# If no arguments are passed to make, it will attempt the default targets
default: hellomake

# Targets to run under 'make all'
all: $(TARGETS)

# List of targets
hellomake: httpclient-block.c httpclient-nonblock.c
	$(CC) -o httpclient-block httpclient-block.c
	$(CC) -o httpclient-nonblock httpclient-nonblock.c

clean:
	$(RM) httpclient-block httpclient-nonblock
