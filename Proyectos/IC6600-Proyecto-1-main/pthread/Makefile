# Compila los binarios pthread reusando tu implementación Huffman
# Probado en Debian/Ubuntu/GCC
# Uso:
#   make           # compila
#   make clean

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I./include -I../huffman/include
LDFLAGS := -lpthread

# fuentes huffman (SIN el main)
HUF_SRCS := ../huffman/src/frecuencias.c ../huffman/src/arbol.c
HUF_OBJS := $(HUF_SRCS:.c=.o)

PTH_SRCS := src/thread_pool.c src/io_utils.c src/huffio.c
PTH_OBJS := $(PTH_SRCS:.c=.o)

BIN_DIR := bin
BINS    := $(BIN_DIR)/huff_compress_pthread $(BIN_DIR)/huff_decompress_pthread

all: $(BINS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/huff_compress_pthread: $(HUF_OBJS) $(PTH_OBJS) src/compress_dir.o | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/huff_decompress_pthread: $(HUF_OBJS) $(PTH_OBJS) src/decompress_dir.o | $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BIN_DIR) */*.o ../huffman/src/*.o
