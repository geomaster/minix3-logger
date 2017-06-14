#pragma once
#include "proto.h"

#define BUFFER_SIZE 4096

#define BUFIO_EOF   0xffffffff
#define BUFIO_ERR   0xfffffffe

typedef struct bufio_t {
	int fd;
	char buffer[BUFFER_SIZE];
	int off;
	int size;
} bufio_t;

bufio_t* bufio_init(int fd);
int bufio_next_char(bufio_t* ctx);
void bufio_free(bufio_t* ctx);
