#Fork
# Compila en Fedora/GCC
CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L \
           -I./include -I../huffman/include
LDFLAGS := 

BIN_DIR := bin
BINS    := $(BIN_DIR)/huff_compress_fork $(BIN_DIR)/huff_decompress_fork

# Implementación Huffman
HUF_SRCS := ../huffman/src/frecuencias.c ../huffman/src/arbol.c
HUF_OBJS := $(HUF_SRCS:.c=.o)

# Fuentes locales de fork
FORK_SRCS := src/huffio.c src/io_utils.c src/compress_dir_fork.c src/decompress_dir_fork.c
FORK_OBJS := $(FORK_SRCS:.c=.o)

all: $(BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/huff_compress_fork: $(HUF_OBJS) src/huffio.o src/io_utils.o src/compress_dir_fork.o | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/huff_decompress_fork: $(HUF_OBJS) src/huffio.o src/io_utils.o src/decompress_dir_fork.o | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) */*.o ../huffman/src/*.o

.PHONY: all clean
