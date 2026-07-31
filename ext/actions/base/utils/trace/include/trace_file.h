/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file trace file interface
 */


#ifndef TRACE_FILE_H_
#define TRACE_FILE_H_

void *trace_file_open(const char *filename);

void trace_file_write(void *handle, const void *buff, uint32_t btw);

void trace_file_close(void *handle);



#endif /* TRACE_FILE_H_ */
