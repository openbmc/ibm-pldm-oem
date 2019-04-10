#ifndef FILEIO_H
#define FILEIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"

/** @brief PLDM Commands in FileIO type
 */
enum pldm_fileio_commands { PLDM_READ_FILE_MEMORY = 0x6 };

/** @brief PLDM Command specific codes
 */
enum pldm_fileio_completion_codes {
	PLDM_INVALID_FILE_HANDLE = 0x80,
	PLDM_DATA_OUT_OF_RANGE = 0x81,
	PLDM_INVALID_READ_LENGTH = 0x82
};

#define PLDM_READ_FILE_MEM_REQ_BYTES 20
#define PLDM_READ_FILE_MEM_RESP_BYTES 5

/* ReadFileIntoMemory */

/** @brief Decode ReadFileIntoMemory commands request data
 *
 *  @param[in] msg - Pointer to PLDM request message payload
 *  @param[in] payload_length - Length of request payload
 *  @param[out] file_handle - A handle to the file
 *  @param[out] offset - Offset to the file at which the read should begin
 *  @param[out] length - Number of bytes to be read
 *  @param[out] address - Memory address where the file content has to be
 *                        written to
 *  @return pldm_completion_codes
 */
int decode_read_file_memory_req(const uint8_t *msg, size_t payload_length,
				uint32_t *file_handle, uint32_t *offset,
				uint32_t *length, uint64_t *address);

/* GetPLDMVersion */

/** @brief Create a PLDM response for ReadFileIntoMemory
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] length - Number of bytes read. This could be less than what the
			 requester asked for.
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param
 *         'msg.body.payload'
 */
int encode_read_file_memory_resp(uint8_t instance_id, uint8_t completion_code,
				 uint32_t length, struct pldm_msg *msg);

#ifdef __cplusplus
}
#endif

#endif /* FILEIO_H */