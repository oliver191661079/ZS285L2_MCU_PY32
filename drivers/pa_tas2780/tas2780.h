/*
* Copyright (C) 2021 ?2022 Texas Instruments Incorporated
*
* All rights reserved not granted herein.
* Limited License.
*
* Texas Instruments Incorporated grants a world-wide, royalty-free,
* non-exclusive license under copyrights and patents it now or
* hereafter owns or controls to make, have made, use, import, offer to
* sell and sell ("Utilize") this software subject to the terms herein.
* With respect to the foregoing patent license, such license is
* granted solely to the extent that any such patent is necessary to
* Utilize the software alone.  The patent license shall not apply to
* any combinations which include this software, other than
* combinations with devices manufactured by or for TI (�TI Devices?.
* No hardware patent is licensed hereunder.
*
* Redistributions must preserve existing copyright notices and
* reproduce this license (including the above copyright notice and the
* disclaimer and (if applicable) source code license limitations
* below) in the documentation and/or other materials provided with the
* distribution
*
* Redistribution and use in binary form, without modification, are
* permitted provided that the following conditions are met:
*
*	* No reverse engineering, decompilation, or disassembly of this
*     software is permitted with respect to any software provided in
*     binary form.
*	* any redistribution and use are licensed by TI for use only with
*     TI Devices.
*	* Nothing shall obligate TI to provide you with source code for
*     the software licensed and provided to you in object code.
*
* If software source code is provided to you, modification and
* redistribution of the source code are permitted provided that the
* following conditions are met:
*
*   * any redistribution and use of the source code, including any
*     resulting derivative works, are licensed by TI for use only
*     with TI Devices.
*   * any redistribution and use of any object code compiled from the
*     source code and any resulting derivative works, are licensed by
*     TI for use only with TI Devices.
*
* Neither the name of Texas Instruments Incorporated nor the names of
* its suppliers may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* DISCLAIMER.
*
* THIS SOFTWARE IS PROVIDED BY TI AND TI�S LICENSORS "AS IS" AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TI AND TI�S LICENSORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __TAS2780_REGISTER_SETTING_H__
#define __TAS2780_REGISTER_SETTING_H__

typedef struct {
    u8_t command;
    u8_t param;
} cfg_reg;

#define CFG_META_SWITCH (255)
#define CFG_META_DELAY (254)
#define CFG_META_BURST (253)

cfg_reg shut_down[] = {
	//BOOK0 PAGE0
	{ 0x00, 0x00 },
	{ 0x7f, 0x00 },
	{ 0x00, 0x00 },
	{ 0x02, 0x0e },
};

cfg_reg voice_power_up_prim[] = {
	//BOOK0 PAGE0
	{ 0x00, 0x00 },
	{ 0x7f, 0x00 },
	{ 0x00, 0x00 },
	{ 0x5c, 0xd9 },
	{ 0x0a, 0x3f },//1 left or 2 right or 3 left+right mix
	{ 0x02, 0x18 },
};

cfg_reg Y_bridge[] = {
	{ 0x00, 0x00 },
	{ 0x02, 0x19 },
	{ 0x02, 0x1a },
	{ 0x00, 0x01 },
	{ 0x19, 0x40 },
	{ 0x00, 0xfd },
	{ 0x0d, 0x0d },
	{ 0x39, 0x54 },
	{ 0x3e, 0x4d },
	{ 0x00, 0x00 },
	{ 0x01, 0x01 },
	{ CFG_META_DELAY, 1},
	{ 0x00, 0x01 },
	{ 0x21, 0x00 },
	{ 0x17, 0xc0 },
	{ 0x00, 0x00 },
	{ 0x02, 0x9a },
	{ 0x03, 0x14 },//AMP_LEVEL[4:0]: 0x14->0x0a->16db, 0x1E->0x0f->18.5db, 0x28->0x14->21db
	{ 0x0a, 0x3a },
	{ 0x0d, 0x03 },
	{ 0x0e, 0x02 },
	{ 0x0f, 0x00 },
	{ 0x1a, 0x00 },//Digital volume control 00h = 0dB \ 01h = -0.5dB \ 02h = -1dB ... \ C8h = -100dB \ Others : Mute
	{ 0x35, 0xb9 },//reg NG_CFG0, NG_EN bit 2 (0b = Disabled, 1b= Enabled)
	{ 0x36, 0xaa },
	{ 0x60, 0x21 },
	{ 0x6a, 0x10 },
	{ 0x71, 0x03 },
	{ 0x00, 0x01 },
	{ 0x00, 0x04 },
	{ CFG_META_BURST, 4},
	{ 0x0c, 0x34 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x10, 0x14 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x14, 0x2b },
	{ 0x33, 0x33 },
	{ 0x33, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x18, 0x10 },
	{ 0x00, 0x00 },
	{ 0x00, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x48, 0x73 },
	{ 0x33, 0x33 },
	{ 0x33, 0x33 },
	{ CFG_META_BURST, 4},
	{ 0x28, 0x39 },
	{ 0x80, 0x00 },
	{ 0x00, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x24, 0x04 },
	{ 0x08, 0x89 },
	{ 0x0f, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x1c, 0x00 },
	{ 0x03, 0xc0 },
	{ 0x00, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x20, 0x40 },
	{ 0xbd, 0xb7 },
	{ 0xc0, 0x00 },
	{ CFG_META_BURST, 4},
	{ 0x2c, 0x2d },
	{ 0x6a, 0x86 },
	{ 0x6f, 0x00 },
	{ 0x00, 0x00 },
	{ CFG_META_BURST, 3},
	{ 0x6c, 0x00 },
	{ 0x00, 0x1a },
	{ CFG_META_BURST, 2},
	{ 0x6f, 0x00 },
	{ 0x96, 0x00 },
	{ 0x02, 0x9a },
	{ 0x00, 0x04 },
	{ CFG_META_BURST, 4},
	{ 0x08, 0x40 },
	{ 0x26, 0xe7 },
	{ 0x3d, 0x00 },
	{ 0x00, 0x00 },
	{ 0x31, 0x37 },
	{ 0x2c, 0x40 },
	{ 0x27, 0x46 },
	{ 0x22, 0x4c }
};

#endif