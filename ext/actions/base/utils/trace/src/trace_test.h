#ifndef TRACE_TEST_H_
#define TRACE_TEST_H_

// << Config for TRACE TEST
//#define TRACE_TEST
#define  TRACE_HEX_TEST

#ifdef TRACE_TEST
//#define TRACE_TEST_WITH_CBUF

#ifdef TRACE_TEST_WITH_CBUF
#define TRACE_STUFF_CBUF
//#define TRACE_MULTI_TASK
#endif

#endif
// >> Config for TRACE TEST

#ifdef TRACE_TEST
extern struct k_sem sem_trace_test;
void trace_test_init(void);
void trace_test_config(int key);
#endif

#endif