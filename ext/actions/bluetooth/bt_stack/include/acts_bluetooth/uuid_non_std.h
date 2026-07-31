/*
 * NOTICE: This file defines UUIDs that are not released by SIG, it is used
 * for debugging and testing and will be removed!
 */

/*
 * PACS
 */
#define BT_UUID_PACS_SINK_PAC             BT_UUID_DECLARE_16(0x2BC9)
#define BT_UUID_PACS_SINK_LOCATIONS       BT_UUID_DECLARE_16(0x2BCA)
#define BT_UUID_PACS_SOURCE_PAC           BT_UUID_DECLARE_16(0x2BCB)
#define BT_UUID_PACS_SOURCE_LOCATIONS     BT_UUID_DECLARE_16(0x2BCC)
#define BT_UUID_PACS_AVAILABLE_CONTEXTS   BT_UUID_DECLARE_16(0x2BCD)
#define BT_UUID_PACS_SUPPORTED_CONTEXTS   BT_UUID_DECLARE_16(0x2BCE)

/*
 * ASCS
 */
#define BT_UUID_ASCS_ASE_CTRL_POINT       BT_UUID_DECLARE_16(0x2BC6)
#define BT_UUID_ASCS_SINK_ASE             BT_UUID_DECLARE_16(0x2BC4)
#define BT_UUID_ASCS_SOURCE_ASE           BT_UUID_DECLARE_16(0x2BC5)

/*
 * TMAS
 */
#define BT_UUID_TMAS_VAL	0x1855
#define BT_UUID_TMAS	BT_UUID_DECLARE_16(BT_UUID_TMAS_VAL)
#define BT_UUID_TMAS_ROLE                 BT_UUID_DECLARE_16(0x2B51)

/*
 * PBP
 */
#define BT_UUID_PUBLIC_BROADCAST_VAL 0x1856
#define BT_UUID_PUBLIC_BROADCAST \
	BT_UUID_DECLARE_16(BT_UUID_PUBLIC_BROADCAST_VAL)

/*
 * TBS
 */
#define BT_UUID_TBS                       BT_UUID_DECLARE_16(0x184B)
#define BT_UUID_GTBS                      BT_UUID_DECLARE_16(0x184C)
/** @def BT_UUID_TBS_PROVIDER_VAL
 *  @brief Telephone Bearer Service Bearer Provider Name value
 */
#define BT_UUID_TBS_PROVIDER_VAL 0x2BB3
/** @def BT_UUID_TBS_PROVIDER
 *  @brief Telephone Bearer Service Bearer Provider Name
 */
#define BT_UUID_TBS_PROVIDER \
	BT_UUID_DECLARE_16(BT_UUID_TBS_PROVIDER_VAL)
/** @def BT_UUID_TBS_UCI_VAL
 *	@brief Telephone Bearer Service Bearer Uniform Caller Identifier value
 */
#define BT_UUID_TBS_UCI_VAL 0x2BB4
/** @def BT_UUID_TBS_UCI
 *	@brief Telephone Bearer Service Bearer Uniform Caller Identifier
 */
#define BT_UUID_TBS_UCI \
	BT_UUID_DECLARE_16(BT_UUID_TBS_UCI_VAL)
/** @def BT_UUID_TBS_TECHNOLOGY_VAL
 *	@brief Telephone Bearer Service Bearer Technology value
 */
#define BT_UUID_TBS_TECHNOLOGY_VAL 0x2BB5
/** @def BT_UUID_TBS_TECHNOLOGY
 *	@brief Telephone Bearer Service Bearer Technology
 */
#define BT_UUID_TBS_TECHNOLOGY \
	BT_UUID_DECLARE_16(BT_UUID_TBS_TECHNOLOGY_VAL)
/** @def BT_UUID_TBS_SCHEMES_VAL
 *	@brief Telephone Bearer Service Bearer URI Schemes Supported List value
 */
#define BT_UUID_TBS_SCHEMES_VAL 0x2BB6
/** @def BT_UUID_TBS_SCHEMES
 *	@brief Telephone Bearer Service Bearer URI Schemes Supported List
 */
#define BT_UUID_TBS_SCHEMES \
	BT_UUID_DECLARE_16(BT_UUID_TBS_SCHEMES_VAL)
/** @def BT_UUID_TBS_STRENGTH_VAL
 *	@brief Telephone Bearer Service Bearer Signal Strength value
 */
#define BT_UUID_TBS_STRENGTH_VAL 0x2BB7
/** @def BT_UUID_TBS_STRENGTH
 *	@brief Telephone Bearer Service Bearer Signal Strength
 */
#define BT_UUID_TBS_STRENGTH \
	BT_UUID_DECLARE_16(BT_UUID_TBS_STRENGTH_VAL)
/** @def BT_UUID_TBS_INTERVAL_VAL
 *	@brief Telephone Bearer Service Bearer Signal Strength Reporting Interval value
 */
#define BT_UUID_TBS_INTERVAL_VAL 0x2BB8
/** @def BT_UUID_TBS_INTERVAL
 *	@brief Telephone Bearer Service Bearer Signal Strength Reporting Interval
 */
#define BT_UUID_TBS_INTERVAL \
	BT_UUID_DECLARE_16(BT_UUID_TBS_INTERVAL_VAL)
/** @def BT_UUID_TBS_CALLS_VAL
 *	@brief Telephone Bearer Service Bearer List Current Calls value
 */
#define BT_UUID_TBS_CALLS_VAL 0x2BB9
/** @def BT_UUID_TBS_CALLS
 *	@brief Telephone Bearer Service Bearer List Current Calls
 */
#define BT_UUID_TBS_CALLS \
	BT_UUID_DECLARE_16(BT_UUID_TBS_CALLS_VAL)
/** @def BT_UUID_TBS_CCID_VAL
 *	@brief Telephone Bearer Service Content Control ID value
 */
#define BT_UUID_TBS_CCID_VAL 0x2BBA
/** @def BT_UUID_TBS_CCID
 *	@brief Telephone Bearer Service Content Control ID
 */
#define BT_UUID_TBS_CCID \
	BT_UUID_DECLARE_16(BT_UUID_TBS_CCID_VAL)
/** @def BT_UUID_TBS_STATUS_VAL
 *	@brief Telephone Bearer Service Status Flags value
 */
#define BT_UUID_TBS_STATUS_VAL 0x2BBB
/** @def BT_UUID_TBS_STATUS
 *	@brief Telephone Bearer Service Status Flags
 */
#define BT_UUID_TBS_STATUS \
	BT_UUID_DECLARE_16(BT_UUID_TBS_STATUS_VAL)
/** @def BT_UUID_TBS_TARGET_VAL
 *	@brief Telephone Bearer Service Incoming Call Target Bearer URI value
 */
#define BT_UUID_TBS_TARGET_VAL 0x2BBC
/** @def BT_UUID_TBS_TARGET
 *	@brief Telephone Bearer Service Incoming Call Target Bearer URI
 */
#define BT_UUID_TBS_TARGET \
	BT_UUID_DECLARE_16(BT_UUID_TBS_TARGET_VAL)
/** @def BT_UUID_TBS_STATE_VAL
 *	@brief Telephone Bearer Service Call State value
 */
#define BT_UUID_TBS_STATE_VAL 0x2BBD
/** @def BT_UUID_TBS_STATE
 *	@brief Telephone Bearer Service Call State
 */
#define BT_UUID_TBS_STATE \
	BT_UUID_DECLARE_16(BT_UUID_TBS_STATE_VAL)
/** @def BT_UUID_TBS_CONTROL_VAL
 *	@brief Telephone Bearer Service Call Control Point value
 */
#define BT_UUID_TBS_CONTROL_VAL 0x2BBE
/** @def BT_UUID_TBS_CONTROL
 *	@brief Telephone Bearer Service Call Control Point
 */
#define BT_UUID_TBS_CONTROL \
	BT_UUID_DECLARE_16(BT_UUID_TBS_CONTROL_VAL)
/** @def BT_UUID_TBS_OPCODES_VAL
 *	@brief Telephone Bearer Service Call Control Point Optional Opcodes value
 */
#define BT_UUID_TBS_OPCODES_VAL 0x2BBF
/** @def BT_UUID_TBS_OPCODES
 *	@brief Telephone Bearer Service Call Control Point Optional Opcodes
 */
#define BT_UUID_TBS_OPCODES \
	BT_UUID_DECLARE_16(BT_UUID_TBS_OPCODES_VAL)
/** @def BT_UUID_TBS_REASON_VAL
 *	@brief Telephone Bearer Service Termination Reason value
 */
#define BT_UUID_TBS_REASON_VAL 0x2BC0
/** @def BT_UUID_TBS_REASON
 *	@brief Telephone Bearer Service Termination Reason
 */
#define BT_UUID_TBS_REASON \
	BT_UUID_DECLARE_16(BT_UUID_TBS_REASON_VAL)
/** @def BT_UUID_TBS_INCOMING_VAL
 *	@brief Telephone Bearer Service Incoming Call value
 */
#define BT_UUID_TBS_INCOMING_VAL 0x2BC1
/** @def BT_UUID_TBS_INCOMING
 *	@brief Telephone Bearer Service Incoming Call
 */
#define BT_UUID_TBS_INCOMING \
	BT_UUID_DECLARE_16(BT_UUID_TBS_INCOMING_VAL)
/** @def BT_UUID_TBS_FRIENDLY_VAL
 *	@brief Telephone Bearer Service Call Friendly Name value
 */
#define BT_UUID_TBS_FRIENDLY_VAL 0x2BC2
/** @def BT_UUID_TBS_FRIENDLY
 *	@brief Telephone Bearer Service Call Friendly Name
 */
#define BT_UUID_TBS_FRIENDLY \
	BT_UUID_DECLARE_16(BT_UUID_TBS_FRIENDLY_VAL)

/*
 * CSIS
 */
#define BT_UUID_CSIS_SIRK                 BT_UUID_DECLARE_16(0x2b84)
#define BT_UUID_CSIS_SIZE                 BT_UUID_DECLARE_16(0x2b85)
#define BT_UUID_CSIS_LOCK                 BT_UUID_DECLARE_16(0x2b86)

/*
 * CAS
 */
#define BT_UUID_CAS_VAL	0x1853
#define BT_UUID_CAS	BT_UUID_DECLARE_16(BT_UUID_CAS_VAL)
