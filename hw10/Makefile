CC := gcc
CFLAGS := -Wall -Wextra -g -std=c99
LDFLAGS := -lm

PROGRAMS := getinodeinfo getinodedata getdirinfo

.PHONY: all clean test valgrind

all: $(PROGRAMS)

getinodeinfo: getinodeinfo.c ext2.c ext2.h
	$(CC) $(CFLAGS) -o $@ getinodeinfo.c ext2.c $(LDFLAGS)

getinodedata: getinodedata.c ext2.c ext2.h
	$(CC) $(CFLAGS) -o $@ getinodedata.c ext2.c $(LDFLAGS)

getdirinfo: getdirinfo.c ext2.c ext2.h
	$(CC) $(CFLAGS) -o $@ getdirinfo.c ext2.c $(LDFLAGS)

test: all
	@bash test.sh

valgrind: all
	@echo "Running valgrind tests..."
	@bash test.sh valgrind

clean:
	rm -f $(PROGRAMS) *.o
	rm -f ext2.img checksum.txt dirinfo.txt
	rm -rf ext2

help:
	@echo "Available targets:"
	@echo "  all       - Build all utilities"
	@echo "  test      - Run test suite"
	@echo "  valgrind  - Run tests with valgrind"
	@echo "  clean     - Clean build artifacts"
	@echo "  help      - Show this message"
