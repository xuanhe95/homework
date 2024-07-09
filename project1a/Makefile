# Define variable CC to be the compiler we want to use
CC = clang

# Define CFLAGS for the flags we will want to use with clang
# -g = debugging flag to enable gdb
# -Wall = show compilation warnings
CFLAGS = -g -Wall

# Define TARGETS to be the targets to be run when calling 'make all'
TARGETS = penn-shredder

# Define PHONY targets to prevent make from confusing the phony target with the same file names
.PHONY: clean all

# If no arguments are passed to make, it will attempt the 'penn-shredder' target
default: penn-shredder

# This runs the 'clean' and 'penn-shredder' targets when 'make all' is run
all: $(TARGETS)

# This will construct the binary 'penn-shredder'
# $^ = names of all the prerequisites, with spaces between them
# $@ = complete name of the target
# $< = name of the first prerequisite
penn-shredder: penn-shredder.c
	$(CC) $(CFLAGS) $< -o $@

# $(RM) is the platform agnostic way to delete a file (here rm -f)
clean:
	$(RM) penn-shredder
