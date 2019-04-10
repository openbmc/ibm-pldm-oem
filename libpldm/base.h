#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <asm/byteorder.h>
#include <stddef.h>
#include <stdint.h>

#include "pldm_types.h"

/** @brief PLDM Types
 */
enum pldm_supported_types {
	PLDM_BASE = 0x00,
};

/** @brief PLDM Commands
 */
enum pldm_supported_commands {
	PLDM_GET_PLDM_VERSION = 0x3,
	PLDM_GET_PLDM_TYPES = 0x4,
	PLDM_GET_PLDM_COMMANDS = 0x5
};

/** @brief PLDM base codes
 */
enum pldm_completion_codes {
	PLDM_SUCCESS = 0x00,
	PLDM_ERROR = 0x01,
	PLDM_ERROR_INVALID_DATA = 0x02,
	PLDM_ERROR_INVALID_LENGTH = 0x03,
	PLDM_ERROR_NOT_READY = 0x04,
	PLDM_ERROR_UNSUPPORTED_PLDM_CMD = 0x05,
	PLDM_ERROR_INVALID_PLDM_TYPE = 0x20
};

enum transfer_op_flag {
	PLDM_GET_NEXTPART = 0,
	PLDM_GET_FIRSTPART = 1,
};

enum transfer_resp_flag {
	PLDM_START = 0x01,
	PLDM_MIDDLE = 0x02,
	PLDM_END = 0x04,
	PLDM_START_AND_END = 0x05,
};

/** @enum MessageType
 *
 *  The different message types supported by the PLDM specification.
 */
typedef enum {
	PLDM_RESPONSE,		   //!< PLDM response
	PLDM_REQUEST,		   //!< PLDM request
	PLDM_RESERVED,		   //!< Reserved
	PLDM_ASYNC_REQUEST_NOTIFY, //!< Unacknowledged PLDM request messages
} MessageType;

#define PLDM_INSTANCE_MAX 31
#define PLDM_MAX_TYPES 64
#define PLDM_MAX_CMDS_PER_TYPE 256

/* Message payload lengths */
#define PLDM_GET_COMMANDS_REQ_BYTES 5
#define PLDM_GET_VERSION_REQ_BYTES 6

/* Response lengths are inclusive of completion code */
#define PLDM_GET_TYPES_RESP_BYTES 9
#define PLDM_GET_COMMANDS_RESP_BYTES 33
/* Response data has only one version and does not contain the checksum */
#define PLDM_GET_VERSION_RESP_BYTES 10

/** @struct pldm_msg_hdr
 *
 * Structure representing PLDM message header fields
 */
struct pldm_msg_hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t instance_id : 5; //!< Instance ID
	uint8_t reserved : 1;    //!< Reserved
	uint8_t datagram : 1;    //!< Datagram bit
	uint8_t request : 1;     //!< Request bit
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t request : 1;     //!< Request bit
	uint8_t datagram : 1;    //!< Datagram bit
	uint8_t reserved : 1;    //!< Reserved
	uint8_t instance_id : 5; //!< Instance ID
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8_t type : 6;       //!< PLDM type
	uint8_t header_ver : 2; //!< Header version
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8_t header_ver : 2;  //!< Header version
	uint8_t type : 6;	//!< PLDM type
#endif
	uint8_t command; //!< PLDM command code
} __attribute__((packed));

/** @struct pldm_msg
 *
 * Structure representing PLDM message
 */
struct pldm_msg {
	struct pldm_msg_hdr hdr; //!< PLDM message header
	uint8_t payload[1];      //!< Starting byte of the message payload
} __attribute__((packed));

/** @struct pldm_header_info
 *
 *  The information needed to prepare PLDM header and this is passed to the
 *  pack_pldm_header and unpack_pldm_header API.
 */
struct pldm_header_info {
	MessageType msg_type;    //!< PLDM message type
	uint8_t instance;	//!< PLDM instance id
	uint8_t pldm_type;       //!< PLDM type
	uint8_t command;	 //!< PLDM command code
	uint8_t completion_code; //!< PLDM completion code, applies for response
};

/**
 * @brief Populate the PLDM message with the PLDM header.The caller of this API
 *        allocates buffer for the PLDM header when forming the PLDM message.
 *        The buffer is passed to this API to pack the PLDM header.
 *
 * @param[in] hdr - Pointer to the PLDM header information
 * @param[out] msg - Pointer to PLDM message header
 *
 * @return 0 on success, otherwise PLDM error codes.
 */
int pack_pldm_header(const struct pldm_header_info *hdr,
		     struct pldm_msg_hdr *msg);

/**
 * @brief Unpack the PLDM header from the PLDM message.
 *
 * @param[in] msg - Pointer to the PLDM message header
 * @param[out] hdr - Pointer to the PLDM header information
 *
 * @return 0 on success, otherwise PLDM error codes.
 */
int unpack_pldm_header(const struct pldm_msg_hdr *msg,
		       struct pldm_header_info *hdr);

#ifdef __cplusplus
}
#endif

#endif /* BASE_H */
