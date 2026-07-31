#ifndef _LOGIC__H_
#define _LOGIC__H_

#ifdef CONFIG_LOGIC_ANALYZER
//#define LOGIC_ANALYZER_UART
//#define LOGIC_ANALYZER_TRACE
//#define LOGIC_ANALYZER_TRACE
#define LOGIC_ANALYZER_LE_AUDIO

void logic_set_1(unsigned char index);
void logic_set_0(unsigned char index);
void logic_set(unsigned char index, bool high);
void logic_switch(unsigned char index);
void logic_init(void);
#endif

#endif /* _LOGIC__H_ */
