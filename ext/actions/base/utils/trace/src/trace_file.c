/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file trace file interface
 */

#include <kernel.h>

#ifdef CONFIG_TRACE_LOGFILE
#include <ff.h>

#define MAX_TRACE_FILE  2

FIL trace_file[MAX_TRACE_FILE];

void *trace_file_open(const char *filename)
{
    uint32_t m = 0;

    FRESULT fr;

    FIL *fp = NULL;

    int i;

    for(i = 0; i < MAX_TRACE_FILE; i++){
        if(trace_file[i].flag == 0){
            fp = &trace_file[i];
            break;
        }
    }

    m = FA_CREATE_ALWAYS | FA_WRITE;

    fr = f_open(fp, filename, m);

    if (fr == FR_OK) {
        printk("%s open succeed %x\n", filename, (uint32_t)fp);
    }else{
    	fp = NULL;
        printk("%s open failure (%d)\n", filename, fr);
    }

    return (void *)fp;
}

void trace_file_write(void *handle, const void *buff, uint32_t btw)
{
    FRESULT fr;

    FIL *fp = (FIL *)handle;

    uint32_t bw;
    uint32_t fsize;

    fsize = fp->fptr;

    fr = f_lseek(fp, fsize);

    if (fr == FR_OK) {

        fr = f_write(fp, buff, btw, &bw);

        if (fr != FR_OK) {
            printk("write failure (%d) handle %x\n", fr, (uint32_t)handle);
        }
    } else {
        printk("f_lseek error %x handle %x\n", fr, (uint32_t)handle);
    }

}

void trace_file_close(void *handle)
{
    f_close((FIL *)handle);
}


static FIL trace_fp;

void hal_trace_file_output(char *s, int len)
{
    trace_file_write((void *)&trace_fp, s, len);
    f_sync(&trace_fp);
}

#endif
