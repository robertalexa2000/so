#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "so_stdio.h"
#include "libso_stdio.h"

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file;

	file = calloc(1, sizeof(SO_FILE));
	if (!file)
		return NULL;

	if (!strcmp(mode, "r"))
		file->flags = O_RDONLY;
	else if (!strcmp(mode, "r+"))
		file->flags = O_RDWR;
	else if (!strcmp(mode, "w"))
		file->flags = O_WRONLY | O_CREAT | O_TRUNC;
	else if (!strcmp(mode, "w+"))
		file->flags = O_RDWR | O_CREAT | O_TRUNC;
	else if (!strcmp(mode, "a"))
		file->flags = O_WRONLY | O_APPEND | O_CREAT;
	else if (!strcmp(mode, "a+"))
		file->flags = O_RDWR | O_APPEND | O_CREAT;
	else
		goto err_mode;

	file->fd = open(pathname, file->flags, 0644);
	if (file->fd < 0)
		goto err_open;

	return file;

err_open:
err_mode:
	free(file);
	return NULL;
}

int so_fclose(SO_FILE *stream)
{
	int ret = 0;

	ret |= so_fflush(stream);
	ret |= close(stream->fd);

	free(stream);
	return ret ? SO_EOF : 0;
}

int so_fgetc(SO_FILE *stream)
{
	int ret;

	if (stream->cursor == stream->size) {
		ret = read(stream->fd, stream->buf, BUF_SIZE);
		if (ret < 0) {
			stream->err = true;
			return SO_EOF;
		}
		if (!ret) {
			stream->eof = true;
			return SO_EOF;
		}

		stream->size = ret;
		stream->cursor = 0;
	}

	stream->op = READ;
	return (int) stream->buf[stream->cursor++];
}

int so_fputc(int c, SO_FILE *stream)
{
	int ret;

	if (stream->cursor == BUF_SIZE) {
		ret = write(stream->fd, stream->buf, BUF_SIZE);
		if (ret <= 0) {
			stream->err = true;
			return SO_EOF;
		}

		memcpy(stream->buf, stream->buf + ret, BUF_SIZE - ret);
		stream->cursor = BUF_SIZE - ret;
	}

	stream->op = WRITE;
	stream->buf[stream->cursor++] = (char) c;
	return c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int num_bytes = nmemb * size;
	char next;
	int i;

	for (i = 0; i < num_bytes; i++) {
		next = (char) so_fgetc(stream);
		if (next == SO_EOF) {
			if (stream->eof)
				return i / size;

			if (stream->err)
				return 0;
		}

		((char *)ptr)[i] = (char) next;
	}

	return num_bytes / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int num_bytes = nmemb * size;
	char next;
	int i;

	for (i = 0; i < num_bytes; i++) {
		next = (char) so_fputc(((char *) ptr)[i], stream);
		if (next == SO_EOF && stream->err)
			return 0;
	}

	return num_bytes / size;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->op == READ)
		stream->cursor = stream->size = 0;
	if (stream->op == WRITE)
		so_fflush(stream);

	return lseek(stream->fd, offset, whence) < 0 ? SO_EOF : 0;
}

long so_ftell(SO_FILE *stream)
{
	int offset;

	offset = lseek(stream->fd, 0, SEEK_CUR);
	if (offset < 0)
		return SO_EOF;
	if (stream->op == READ)
		offset -= stream->size - stream->cursor;
	if (stream->op == WRITE)
		offset += stream->cursor;

	return offset;
}

static int xwrite(SO_FILE *stream)
{
	int bytes_written = 0, bytes_written_now;

	do {
		bytes_written_now = write(stream->fd, stream->buf + bytes_written,
					  stream->cursor - bytes_written);
		if (bytes_written_now <= 0)
			return SO_EOF;
		bytes_written += bytes_written_now;
	} while(bytes_written != stream->cursor);

	return bytes_written;
}

int so_fflush(SO_FILE *stream)
{
	int ret;

	if (stream->op == WRITE && stream->cursor) {
		ret = xwrite(stream);
		if (ret <= 0)
			return SO_EOF;

		stream->cursor = 0;
	}

	return 0;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_feof(SO_FILE *stream)
{
	return stream->eof;
}

int so_ferror(SO_FILE *stream)
{
	return stream->err;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *stream = NULL;
	int fds[2], fd;
	int ret;

	ret = pipe(fds);
	if (ret < 0)
		return NULL;

	ret = fork();

	switch (ret) {
	case -1:
		break;
	case 0:
		if (!strcmp(type, "r")) {
			close(fds[READ]);
			dup2(fds[WRITE], STDOUT_FILENO);
		} else {
			close(fds[WRITE]);
			dup2(fds[READ], STDIN_FILENO);
		}

		execl("/bin/sh", "sh", "-c", command, NULL);
		break;
	default:
		if (!strcmp(type, "r")) {
			close(fds[WRITE]);
			fd = fds[READ];
		} else {
			close(fds[READ]);
			fd = fds[WRITE];
		}

		stream = calloc(1, sizeof(SO_FILE));
		if (!stream)
			break;
		stream->pid = ret;
		stream->fd = fd;

		break;
	}

	return stream;
}

int so_pclose(SO_FILE *stream)
{
	int status, ret = 0;

	ret |= so_fflush(stream);
	ret |= close(stream->fd);
	ret |= waitpid(stream->pid, &status, 0);

	free(stream);
	return ret == -1 ? ret : 0;
}
