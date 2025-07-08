CC = gcc
CFLAGS = -DNCURSES_WIDECHAR=1 -D_POSIX_C_SOURCE=200809L -Wall -pedantic -std=c99 -g
LIBS = -lncursesw
TARGET = monkype

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

