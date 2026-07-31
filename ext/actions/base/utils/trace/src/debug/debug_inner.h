
#ifndef _DEBUG_INNER_H_
#define _DEBUG_INNER_H_

#define LINESEP_FORMAT_WINDOWS  0
#define LINESEP_FORMAT_UNIX     1

/* PRINT options */

#define PRINT_BLOCK_SIZE_SHIFT  (0)
#define PRINT_BLOCK_SIZE_MASK   (0xffff << PRINT_BLOCK_SIZE_SHIFT)

#define PRINT_TIME_STAMP_ENABLE (1 << 16)

#define PRINT_LINESEP_FMT_SHIFT (17)
#define PRINT_LINESEP_FMT_MASK  (0x03 << PRINT_LINESEP_FMT_SHIFT)

/* PRINT options END */

/* DUMP options */

#define DUMP_LEADER_MAX_LEN     (16)

#define DUMP_LINE_UNITS_SHIFT   (0)
#define DUMP_LINE_UNITS_MASK    (0xFF << DUMP_LINE_UNITS_SHIFT)

#define DUMP_PREFIX_SHIFT       (8)
#define DUMP_PREFIX_MASK        (1 << DUMP_PREFIX_SHIFT)
#define DUMP_PREFIX_NUL         (0 << DUMP_PREFIX_SHIFT)
#define DUMP_PREFIX_0X          (1 << DUMP_PREFIX_SHIFT)

#define DUMP_SPACE_SHIFT        (9)
#define DUMP_SPACE_MASK         (3 << DUMP_SPACE_SHIFT)
#define DUMP_SPACE_SPACE        (0 << DUMP_SPACE_SHIFT)
#define DUMP_SPACE_NUL          (1 << DUMP_SPACE_SHIFT)
#define DUMP_SPACE_COMMA        (2 << DUMP_SPACE_SHIFT)

#define DUMP_LINE_TAIL_UNIT_SPACE_SHIFT  (11)
#define DUMP_LINE_TAIL_UNIT_SPACE_MASK   (1 << DUMP_LINE_TAIL_UNIT_SPACE_SHIFT)
#define DUMP_LINE_TAIL_UNIT_SPACE_NO     (0 << DUMP_LINE_TAIL_UNIT_SPACE_SHIFT)
#define DUMP_LINE_TAIL_UNIT_SPACE_YES    (1 << DUMP_LINE_TAIL_UNIT_SPACE_SHIFT)

#define DUMP_UNIT_SHIFT         (12)
#define DUMP_UNIT_MASK          (3 << DUMP_UNIT_SHIFT)
#define DUMP_UNIT_BYTE          (0 << DUMP_UNIT_SHIFT)
#define DUMP_UNIT_HALF          (1 << DUMP_UNIT_SHIFT)
#define DUMP_UNIT_WORD          (2 << DUMP_UNIT_SHIFT)

#define DUMP_ADDR_SHIFT         (14)
#define DUMP_ADDR_MASK          (3 << DUMP_ADDR_SHIFT)
#define DUMP_ADDR_NUL           (0 << DUMP_ADDR_SHIFT)
#define DUMP_ADDR_TITLE         (1 << DUMP_ADDR_SHIFT)
#define DUMP_ADDR_LINE          (2 << DUMP_ADDR_SHIFT)

/* DUMP options END */

#define REG_UART_CTL(a)    ((a) + 0x00)
#define REG_UART_RXDAT(a)  ((a) + 0x04)
#define REG_UART_TXDAT(a)  ((a) + 0x08)
#define REG_UART_STA(a)    ((a) + 0x0C)
#define REG_UART_BR(a)     ((a) + 0x10)

#ifndef BIT
#define BIT(n)  (1UL << (n))
#endif

typedef enum
{
    PRINT_BUF_NO_START = 0,
    PRINT_BUF_PRINTING = 1,
    PRINT_BUF_PRINT_CP = 2,
} print_buf_status_e;

#define PRINT_MODE_CALL_START  0
#define PRINT_MODE_IRQ_RELOAD  1

#endif
