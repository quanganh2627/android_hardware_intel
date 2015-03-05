CC = gcc
CFLAGS = -Ilib/include/ -g -O2 -m32 -Wall
LDFLAGS = -L. -lcgpt
STATIC_LIBRARY_FILES = $(wildcard lib/*.c)
STATIC_LIBRARY_OBJ = $(addprefix lib/,$(notdir $(STATIC_LIBRARY_FILES:.c=.o)))
STATIC_LIBRARY_OUT = libcgpt.a
CGPT_BINARY_FILES = $(wildcard gfdisk/*.c)
CGPT_BINARY_OBJ = $(STATIC_LIBRARY_OUT) $(addprefix gfdisk/,$(notdir $(CGPT_BINARY_FILES:.c=.o)))
CGPT_BINARY_OUT = cgpt 

all: $(CGPT_BINARY_OUT)

$(STATIC_LIBRARY_OUT): $(STATIC_LIBRARY_OBJ)
	ar rcs $(STATIC_LIBRARY_OUT) $(STATIC_LIBRARY_OBJ)

$(CGPT_BINARY_OUT): $(CGPT_BINARY_OBJ)
	$(CC) $(CFLAGS) $(CGPT_BINARY_OBJ) -o $(CGPT_BINARY_OUT) $(LDFLAGS)
	strip $(CGPT_BINARY_OUT)

lib/%.o : lib/%.c
	$(CC) $(CFLAGS) -o $@  -c $<

