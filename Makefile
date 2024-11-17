CC = gcc
CFLAGS = -Wall -g
TARGET = out/tiny-js-game

all: $(TARGET)

debug: CFLAGS += -DDEBUG
debug: $(TARGET)

$(TARGET): out/main.o
	$(CC) $(CFLAGS) -o $(TARGET) build/main.o build/duktape.o -Ilib/SDL/build/../include -Llib/SDL/build/ -Wl,-rpath,lib/SDL/build/ -lSDL3

out/main.o: src/main.c | out
	$(CC) $(CFLAGS) -c -Isrc src/duktape/duktape.c src/main.c -lm
	mv duktape.o build/duktape.o
	mv main.o build/main.o

out:
	mkdir -p out
	mkdir -p build

clean:
	rm -rf out
	rm -rf build

.PHONY: all clean
