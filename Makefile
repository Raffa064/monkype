CC = gcc
CFLAGS = -Wall -pedantic -std=c99 -g
LIBS = -lncursesw
TARGET = monkype

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

