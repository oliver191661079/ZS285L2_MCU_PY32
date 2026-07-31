/*
 */

extern void __start(void);
extern void trap(void);
extern void trap0_handler(void);
extern void vectorirq_handler(void);
extern void _timer_int_handler(void);
extern void tspend_handler(void);


#if defined (__cplusplus)
extern "C" {
#endif

extern void (* g_exp_table[])(void);
__attribute__((used, section(".exp_table")))
void (* g_exp_table[])(void) = {
    __start,           /* 0, Boot entry               */
    trap,              /* 1, default exception entry  */
    trap,              /* 2, default exception entry  */
    trap,              /* 3, default exception entry  */
    trap,              /* 4, default exception entry  */
    trap,              /* 5, default exception entry  */
    trap,              /* 6, default exception entry  */
    trap,              /* 7, default exception entry  */
    trap,              /* 8, default exception entry  */
    trap,              /* 9, default exception entry  */
    trap,              /* 10, default exception entry */
    trap,              /* 11, default exception entry */
    trap,              /* 12, default exception entry */
    trap,              /* 13, default exception entry */
    trap,              /* 14, default exception entry */
    trap,              /* 15, default exception entry */
    trap0_handler,     /* 16, default exception entry */
    trap,              /* 17, default exception entry */
    trap,              /* 18, default exception entry */
    trap,              /* 19, default exception entry */
    trap,              /* 20, default exception entry */
    trap,              /* 21, default exception entry */
    tspend_handler,    /* 22, default exception entry */
    trap,              /* 23, default exception entry */
    trap,              /* 24, default exception entry */
    trap,              /* 25, default exception entry */
    trap,              /* 26, default exception entry */
    trap,              /* 27, default exception entry */
    trap,              /* 28, default exception entry */
    trap,              /* 29, default exception entry */
    trap,              /* 30, default exception entry */
    trap,              /* 31, default exception entry */

    vectorirq_handler, /* 32 default interrupt entry  */
    vectorirq_handler, /* 33 default interrupt entry  */
    vectorirq_handler, /* 34 default interrupt entry  */
    vectorirq_handler, /* 35 default interrupt entry  */
    vectorirq_handler, /* 36 default interrupt entry  */
    vectorirq_handler, /* 37 default interrupt entry  */
    vectorirq_handler, /* 38 default interrupt entry  */
    vectorirq_handler, /* 39 default interrupt entry  */
    vectorirq_handler, /* 40 default interrupt entry  */
    vectorirq_handler, /* 41 default interrupt entry  */
    vectorirq_handler, /* 42 default interrupt entry  */
    vectorirq_handler, /* 43 default interrupt entry  */
    vectorirq_handler, /* 44 default interrupt entry  */
    vectorirq_handler, /* 45 default interrupt entry  */
    vectorirq_handler, /* 46 default interrupt entry  */
    vectorirq_handler, /* 47 default interrupt entry  */
    vectorirq_handler, /* 48 default interrupt entry  */
    vectorirq_handler, /* 49 default interrupt entry  */
    vectorirq_handler, /* 50 default interrupt entry  */
    vectorirq_handler, /* 51 default interrupt entry  */
    vectorirq_handler, /* 52 default interrupt entry  */
    vectorirq_handler, /* 53 default interrupt entry  */
    vectorirq_handler, /* 54 default interrupt entry  */
    vectorirq_handler, /* 55 default interrupt entry  */
    vectorirq_handler, /* 56 default interrupt entry  */
    vectorirq_handler, /* 57 default interrupt entry  */
    vectorirq_handler, /* 58 default interrupt entry  */
    vectorirq_handler, /* 59 default interrupt entry  */
    vectorirq_handler, /* 60 default interrupt entry  */
    vectorirq_handler, /* 61 default interrupt entry  */
    vectorirq_handler, /* 62 default interrupt entry  */
    vectorirq_handler, /* 63 default interrupt entry  */

    vectorirq_handler, /* 64, default interrupt entry */
    vectorirq_handler, /* 65, default interrupt entry */
    vectorirq_handler, /* 66, default interrupt entry */
    vectorirq_handler, /* 67, default interrupt entry */
    vectorirq_handler, /* 68, default interrupt entry */
    vectorirq_handler, /* 69, default interrupt entry */
    vectorirq_handler, /* 70, default interrupt entry */
    vectorirq_handler, /* 71, default interrupt entry */
    vectorirq_handler, /* 72, default interrupt entry */
    vectorirq_handler, /* 73, default interrupt entry */
};

