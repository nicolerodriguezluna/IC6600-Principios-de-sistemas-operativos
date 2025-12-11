CC = gcc
CFLAGS = -Wall -Iinclude
SRCDIR = src
INCDIR = include
LIB_SOURCES = $(SRCDIR)/arbol.c $(SRCDIR)/frecuencias.c
LIB_OBJECTS = $(LIB_SOURCES:.c=.o)
TARGET_LIB = libhuffman.a

TERMINAL_SOURCE = $(SRCDIR)/huffman.c
TERMINAL_OBJECT = $(TERMINAL_SOURCE:.c=.o)
TARGET_TERMINAL = huffman-terminal

all: $(TARGET_LIB) $(TARGET_TERMINAL)

$(TARGET_LIB): $(LIB_OBJECTS)
	ar rcs $@ $^

$(TARGET_TERMINAL): $(TERMINAL_OBJECT) $(TARGET_LIB)
	$(CC) $(CFLAGS) -o $@ $(TERMINAL_OBJECT) -L. -lhuffman

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIB_OBJECTS) $(TERMINAL_OBJECT) $(TARGET_LIB) $(TARGET_TERMINAL)

.PHONY: all clean