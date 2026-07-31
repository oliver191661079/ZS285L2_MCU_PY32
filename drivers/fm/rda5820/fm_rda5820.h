
#ifndef _FM_RDA5820_H
#define _FM_RDA5820_H

#define FM_I2C_DEV0_ADDRESS 	0x11
#define FM_I2C_DEV1_ADDRESS 	0x10


typedef enum
{
    eFM_RECEIVER = 0,                //5800,5802,5804
    eFM_TRANSMITTER = 1                //5820,5820NS
}E_RADIO_WORK;


typedef struct {
    unsigned char reg;
    unsigned short int value;
}RDAFM_REG_t;

#define rda_delay(ms)      		k_sleep(ms)


/*************register address table**************/
#define  RDA_REGISTER_00     0x00
#define  RDA_REGISTER_02     0x02
#define  RDA_REGISTER_03     0x03
#define  RDA_REGISTER_04     0x04
#define  RDA_REGISTER_05     0x05
#define  RDA_REGISTER_06     0x06
#define  RDA_REGISTER_07     0x07
#define  RDA_REGISTER_14     0x14
#define  RDA_REGISTER_15     0x15
#define  RDA_REGISTER_16     0x16
#define  RDA_REGISTER_18     0x18
#define  RDA_REGISTER_19     0x19
#define  RDA_REGISTER_1c     0x1c
#define  RDA_REGISTER_1e     0x1c
#define  RDA_REGISTER_20     0x20
#define  RDA_REGISTER_25     0x25
#define  RDA_REGISTER_27     0x27
#define  RDA_REGISTER_28     0x28
#define  RDA_REGISTER_2d     0x2d
#define  RDA_REGISTER_2e     0x2e
#define  RDA_REGISTER_2f     0x2f
#define  RDA_REGISTER_36     0x36
#define  RDA_REGISTER_37     0x37
#define  RDA_REGISTER_38     0x38
#define  RDA_REGISTER_5b     0x5b
#define  RDA_REGISTER_5c     0x5c
#define  RDA_REGISTER_0a     0x0a
#define  RDA_REGISTER_0b     0x0b
#define  RDA_REGISTER_0c     0x0c
#define  RDA_REGISTER_0d     0x0d
#define  RDA_REGISTER_0e     0x0e
#define  RDA_REGISTER_0f     0x0f
#define  RDA_REGISTER_40     0x40
#define  RDA_REGISTER_41     0x41
#define  RDA_REGISTER_67     0x67
#define  RDA_REGISTER_68     0x68

#endif

