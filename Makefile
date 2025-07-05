CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g
LIBS = -lncurses
TARGET = monkype

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

