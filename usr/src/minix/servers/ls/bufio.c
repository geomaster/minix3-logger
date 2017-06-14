#include "bufio.h"
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

bufio_t* bufio_init(int fd) {
	bufio_t* ctx = (bufio_t*)malloc(sizeof(bufio_t));
	if (!ctx) {
		goto alloc_fail;
	}

	ssize_t nread = read(fd, ctx->buffer, BUFFER_SIZE);
	if (nread == -1) {
		goto read_fail;
	}

	ctx->fd = fd;
	ctx->off = 0;
	ctx->size = (int)nread;

	return ctx;
		
read_fail:
	free(ctx);

alloc_fail:
	return NULL;
}

int bufio_next_char(bufio_t* ctx) {
	if (ctx->off >= ctx->size && ctx->size < BUFFER_SIZE) {
		// Since size < BUFFER_SIZE, this was the last chunk, so there's no more
		// bytes left in the file.
		return BUFIO_EOF;
	} else if (ctx->off >= ctx->size) {
		ssize_t nread = read(ctx->fd, ctx->buffer, BUFFER_SIZE);
		if (nread == -1) {
			ctx->size = 0;
			ctx->off = 0;

			return BUFIO_ERR;
		}

		ctx->off = 0;
		ctx->size = (int)nread;
	}

	return ctx->buffer[ctx->off++];
}

void bufio_free(bufio_t* ctx) {
	if (ctx) {
		free(ctx);
	}
}
