#ifndef __CONFIG_UART_AT_STREAM_H
#define __CONFIG_UART_AT_STREAM_H

//charactor backspace
#define AT_PARSER_BS_C                   0x08
//charactor carriage return
#define AT_PARSER_CR_C                   0x0D
//charactor new line
#define AT_PARSER_LF_C                   0x0A
//charactor delete
#define AT_PARSER_DEL_C                  0x7F

//start with printable character
#define AT_PARSER_SPACE                  (' ')
#define AT_PARSER_EQUAL                  ('=')
#define AT_PARSER_COMMA                  (')')
#define AT_PARSER_SEMICOLON              (';')
#define AT_PARSER_COLON                  (':')
#define AT_PARSER_AT                     ('@')
#define AT_PARSER_HAT                    ('^')
#define AT_PARSER_DOUBLE_QUOTE           ('"')
#define AT_PARSER_QUESTION_MARK          ('?')
#define AT_PARSER_EXCLAMATION_MARK       ('!')
#define AT_PARSER_FORWARD_SLASH          ('/')
#define AT_PARSER_L_ANGLE_BRACKET        ('<')
#define AT_PARSER_R_ANGLE_BRACKET        ('>')
#define AT_PARSER_L_SQ_BRACKET           ('[')
#define AT_PARSER_R_SQ_BRACKET           (']')
#define AT_PARSER_L_CURLY_BRACKET        ('{')
#define AT_PARSER_R_CURLY_BRACKET        ('}')
#define AT_PARSER_CHAR_STAR              ('*')
#define AT_PARSER_CHAR_POUND             ('#')
#define AT_PARSER_CHAR_AMPSAND           ('&')
#define AT_PARSER_CHAR_PERCENT           ('%')
#define AT_PARSER_CHAR_PLUS              ('+')
#define AT_PARSER_CHAR_MINUS             ('-')
#define AT_PARSER_CHAR_DOT               ('.')
#define AT_PARSER_CHAR_ULINE             ('_')
#define AT_PARSER_CHAR_TILDE             ('~')
#define AT_PARSER_CHAR_REVERSE_SOLIDUS   ('\\')
#define AT_PARSER_CHAR_VERTICAL_LINE     ('|')
#define AT_PARSER_END_OF_STRING_CHAR     ('\0')
#define AT_PARSER_CHAR_0                 ('0')
#define AT_PARSER_CHAR_1                 ('1')
#define AT_PARSER_CHAR_2                 ('2')
#define AT_PARSER_CHAR_3                 ('3')
#define AT_PARSER_CHAR_4                 ('4')
#define AT_PARSER_CHAR_5                 ('5')
#define AT_PARSER_CHAR_6                 ('6')
#define AT_PARSER_CHAR_7                 ('7')
#define AT_PARSER_CHAR_8                 ('8')
#define AT_PARSER_CHAR_9                 ('9')
#define AT_PARSER_CHAR_A                 ('A')
#define AT_PARSER_CHAR_B                 ('B')
#define AT_PARSER_CHAR_C                 ('C')
#define AT_PARSER_CHAR_D                 ('D')
#define AT_PARSER_CHAR_E                 ('E')
#define AT_PARSER_CHAR_F                 ('F')
#define AT_PARSER_CHAR_G                 ('G')
#define AT_PARSER_CHAR_H                 ('H')
#define AT_PARSER_CHAR_I                 ('I')
#define AT_PARSER_CHAR_J                 ('J')
#define AT_PARSER_CHAR_K                 ('K')
#define AT_PARSER_CHAR_L                 ('L')
#define AT_PARSER_CHAR_M                 ('M')
#define AT_PARSER_CHAR_N                 ('N')
#define AT_PARSER_CHAR_O                 ('O')
#define AT_PARSER_CHAR_P                 ('P')
#define AT_PARSER_CHAR_Q                 ('Q')
#define AT_PARSER_CHAR_R                 ('R')
#define AT_PARSER_CHAR_S                 ('S')
#define AT_PARSER_CHAR_T                 ('T')
#define AT_PARSER_CHAR_U                 ('U')
#define AT_PARSER_CHAR_V                 ('V')
#define AT_PARSER_CHAR_W                 ('W')
#define AT_PARSER_CHAR_X                 ('X')
#define AT_PARSER_CHAR_Y                 ('Y')
#define AT_PARSER_CHAR_Z                 ('Z')
#define AT_PARSER_char_a                 ('a')
#define AT_PARSER_char_b                 ('b')
#define AT_PARSER_char_c                 ('c')
#define AT_PARSER_char_d                 ('d')
#define AT_PARSER_char_e                 ('e')
#define AT_PARSER_char_f                 ('f')
#define AT_PARSER_char_g                 ('g')
#define AT_PARSER_char_h                 ('h')
#define AT_PARSER_char_i                 ('i')
#define AT_PARSER_char_j                 ('j')
#define AT_PARSER_char_k                 ('k')
#define AT_PARSER_char_l                 ('l')
#define AT_PARSER_char_m                 ('m')
#define AT_PARSER_char_n                 ('n')
#define AT_PARSER_char_o                 ('o')
#define AT_PARSER_char_p                 ('p')
#define AT_PARSER_char_q                 ('q')
#define AT_PARSER_char_r                 ('r')
#define AT_PARSER_char_s                 ('s')
#define AT_PARSER_char_t                 ('t')
#define AT_PARSER_char_u                 ('u')
#define AT_PARSER_char_v                 ('v')
#define AT_PARSER_char_w                 ('w')
#define AT_PARSER_char_x                 ('x')
#define AT_PARSER_char_y                 ('y')
#define AT_PARSER_char_z                 ('z')
#define AT_PARSER_R_BRACKET              (')')
#define AT_PARSER_L_BRACKET              ('(')
#define AT_PARSER_MONEY                  ('$')


typedef enum {
  	AT_STREAM_AT_COMMAND,
  	AT_STREAM_PASSTHROUGH_FLAG,
	AT_STREAM_PASSTHROUGH_DATA,
	AT_STREAM_OTA_DATA_FLAG,
	AT_STREAM_OTA_DATA_DATA,
} at_stream_state_e;


typedef enum{
	AT_CHAR_MODE_WITH_NULL,
	AT_CHAR_MODE_WITH_CR,
	AT_CHAR_MODE_WITH_LF,
	AT_CHAR_MODE_WITH_CRLF,
}at_char_mode_e;

struct uart_at_stream_param{
	const char *dev_name;
	void *read_sem;
	uint32_t recv_buf_size;
	uint8_t obj_type; //0:at data 1:passthrough data
	uint8_t end_char_mode;
	void *priv_data;
	void (*read_cb)(void *priv_data);
};

void *at_stream_init(struct uart_at_stream_param *param);

int at_stream_open(void *handle);

int at_stream_write(void *handle, unsigned char *buf, int num);

int at_stream_read(void *handle, unsigned char *buf, int num);

int at_stream_get_length(void *handle);

int at_stream_flush(void *handle);

int at_stream_close(void *handle);

int at_stream_destory(void *handle);

#endif
