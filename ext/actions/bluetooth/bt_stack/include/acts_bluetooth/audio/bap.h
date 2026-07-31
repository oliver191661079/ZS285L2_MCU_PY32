#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_BAP_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_BAP_H_

#include <toolchain.h>
#include <zephyr/types.h>

#include <bluetooth/audio/codec.h>



// TODO: Broadcast Audio Announcements
struct baa {
	uint8_t len;
	uint8_t type;
	uint16_t uuid;
	uint8_t broadcast_id[3];
};
// TODO: Basic Audio Announcements

/* Level 3: BIS level */
struct bis_struct {
	sys_snode_t node;
	uint8_t bis_index;

	/* NOTE: prior to level 2 for the same type */
	uint8_t cc_len;
	uint8_t sampling;
	uint8_t duration;
	uint32_t locations;
	uint16_t octets;
	uint8_t blocks;
};

/* Level 2: Subgroup level */
struct subgroup_struct {
	uint8_t num_bis;	// no need
//	struct codec_id codec;
	uint8_t coding_format;
	uint16_t company_id;
	uint16_t vs_codec_id;

	uint8_t cc_len;
	uint8_t sampling;
	uint8_t duration;
	uint32_t locations;
	uint16_t octets;
	uint8_t blocks;

	uint8_t meta_len;
	uint16_t audio_contexts;	// TODO: including CCID

	sys_snode_t node;
	sys_slist_t bis;
};

/* Level 1: Group level */
struct base_struct {
	uint32_t delay;	// uint8_t delay[3];
	uint8_t num_subgroups;	// no need

	sys_slist_t subgroups;

	uint8_t buf[256];
};

#if 1
#define BT_BAS_STATE_IDLE	0x01
#define BT_BAS_STATE_CONFIGURED	0x02
#define BT_BAS_STATE_STREAMING	0x03

#define BT_BAS_OPCODE_CONFIGURE	0x01
#define BT_BAS_OPCODE_RECONFIGURE	0x02
#define BT_BAS_OPCODE_ESTABLISH	0x03
#define BT_BAS_OPCODE_UPDATE	0x04
#define BT_BAS_OPCODE_DISABLE	0x05
#define BT_BAS_OPCODE_RELEASE	0x06
#endif



#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_BAP_H_ */
