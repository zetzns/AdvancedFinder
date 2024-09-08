CC = gcc
CFLAGS = -Wall -Wextra -Werror -fPIC
LDFLAGS = -ldl

TARGET = main
PLUGIN = libzan.so

all: $(TARGET) $(PLUGIN)

$(TARGET): main.o
	$(CC) -o $@ $^ $(LDFLAGS)

$(PLUGIN): plugin.o
	$(CC) $(CFLAGS) -shared -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -o $@ -c $<

plugin.o: plugin.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o $(TARGET) $(PLUGIN)

.PHONY: all clean
