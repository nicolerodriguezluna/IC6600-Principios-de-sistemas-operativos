CC = gcc
CFLAGS = -Wall -Iinclude -I../huffman/include
LDFLAGS = -L../huffman
LDLIBS = -lhuffman
SRCDIR = src
INCDIR = include
SOURCES = $(SRCDIR)/compresor.c $(SRCDIR)/descompresor.c $(SRCDIR)/io_handler.c
COMPRESOR_OBJ = $(SRCDIR)/compresor.o
DECOMPRESOR_OBJ = $(SRCDIR)/descompresor.o
IO_HANDLER_OBJ = $(SRCDIR)/io_handler.o
TARGET_COMPRESOR = huffman-compresor
TARGET_DECOMPRESOR = huffman-descompresor

all: $(TARGET_COMPRESOR) $(TARGET_DECOMPRESOR)

$(TARGET_COMPRESOR): $(COMPRESOR_OBJ) $(IO_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TARGET_DECOMPRESOR): $(DECOMPRESOR_OBJ) $(IO_HANDLER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(TARGET_COMPRESOR) $(TARGET_DECOMPRESOR)

.PHONY: all clean