CC = gcc
CFLAGS = -Wall -g
TARGET = out/tiny-js-game

all: $(TARGET)

$(TARGET): build/main.o | out
	$(CC) $(CFLAGS) -o $(TARGET) build/main.o

build/main.o: src/main.c | build
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

out build:
	mkdir -p $@

clean:
	rm -f $(TARGET) build/main.o
	rm -rf out build

.PHONY: all clean
