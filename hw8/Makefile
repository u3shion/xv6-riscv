CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = 
TARGET = log_server
SOURCES = log_server.c signals.c daemon.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = log_server.h

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

run: $(TARGET)
	./$(TARGET)

run-daemon: $(TARGET)
	./$(TARGET) -d