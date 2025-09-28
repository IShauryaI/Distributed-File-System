CC = gcc
CFLAGS = -Wall -Wextra -std=c99

all: bin/S1 bin/S2 bin/S3 bin/S4 bin/s25client

bin/S1: src/S1.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/S1 src/S1.c

bin/S2: src/S2.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/S2 src/S2.c

bin/S3: src/S3.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/S3 src/S3.c

bin/S4: src/S4.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/S4 src/S4.c

bin/s25client: src/s25client.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/s25client src/s25client.c

clean:
	rm -rf bin

.PHONY: all clean
