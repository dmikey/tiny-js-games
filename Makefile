CC = gcc
CFLAGS = -Wall -g -Isrc
TARGET = out/tiny-js-game

all: $(TARGET)

$(TARGET): out/main.o | out
	$(CC) $(CFLAGS) -o $(TARGET) src/duktape/duktape.c src/main.c -lm

out/main.o: src/main.c | out
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

out:
	mkdir -p out
	mkdir -p build

clean:
	rm -f $(TARGET) out/main.o
	rm -rf out
	rm -rf build

.PHONY: all clean
