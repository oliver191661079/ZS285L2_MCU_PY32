/*
 * Copyright (c) 2020, Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stream.h>
#include <mem_manager.h>
#include <arithmetic_storage_io.h>

static int _storage_stream_read(void *buf, int size, int count, storage_io_t *io)
{
	if (!io || !io->hnd)
		return -EINVAL;

	if (size != 1)
		count *= size;

	return stream_read((io_stream_t)io->hnd, buf, count);
}

static int _storage_stream_write(void *buf, int size, int count, storage_io_t *io)
{
	if (!io || !io->hnd)
		return -EINVAL;

	if (size != 1)
		count *= size;

	return stream_write((io_stream_t)io->hnd, buf, count);
}

static int _storage_stream_seek(storage_io_t *io, int offset, int whence)
{
	if (!io || !io->hnd)
		return -EINVAL;

	/* STORAGEIO_SEEK_xxx equal to io_stream_t SEEK_DIR_xxx */
	return stream_seek((io_stream_t)io->hnd, offset, whence);
}

static int _storage_stream_tell(storage_io_t *io, int mode)
{
	io_stream_t stream;

	if (!io || !io->hnd)
		return -EINVAL;

	stream = (io_stream_t)io->hnd;
	switch (mode) {
	case STORAGEIO_TELL_CUR:
		return stream_tell(stream);
	case STORAGEIO_TELL_END:
		return stream->total_size;
	default:
		return -EINVAL;
	}
}

storage_io_t *storage_io_wrap_stream(void *stream)
{
	storage_io_t *io = mem_malloc(sizeof(storage_io_t));
	if (io) {
		io->hnd = stream;
		io->read = _storage_stream_read;
		io->write = _storage_stream_write;
		io->seek = _storage_stream_seek;
		io->tell = _storage_stream_tell;
	}

	return io;
}

void storage_io_unwrap_stream(storage_io_t *io)
{
	mem_free(io);
}
