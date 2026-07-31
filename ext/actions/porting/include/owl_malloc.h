/*
********************************************************************************
*                       noya131---upgrade.bin
*                (c) Copyright 2002-2007, Actions Co,Ld. 
*                        All Right Reserved 
*
* FileName: upgrade.h     Author: huang he        Date:2007/12/24
* Description: defines macros for upgrading
* Others:      
* History:         
* <author>    <time>       <version >    <desc>
*   huang he    2007/12/24       1.0         build this file
********************************************************************************
*/ 

#ifndef _OWL_MALLOC_H_
#define _OWL_MALLOC_H_

int owl_init_heap(unsigned long startaddr, unsigned int heap_buf_size);
void *owl_malloc(unsigned int num_bytes);
void owl_free(void *p);
void *owl_calloc(unsigned int n_elements, unsigned int elem_size);
void *owl_realloc(void* oldMem, unsigned int bytes);

#endif
