#ifndef FILEIO_H
#define FILEIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "base.h"

#define PLDM_IBM_OEM_TYPE 0x3F

/** @brief PLDM Commands in IBM OEM type
 */
enum pldm_fileio_commands { PLDM_READ_FILE_INTO_MEMORY = 0x6 };

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

/** @brief Decode ReadWriteFileIntoMemory commands request data
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
int decode_rw_file_memory_req(const uint8_t *msg, size_t payload_length,
				uint32_t *file_handle, uint32_t *offset,
				uint32_t *length, uint64_t *address);

/** @brief Create a PLDM response for ReadWriteFileIntoMemory
 *
 *  @param[in] instance_id - Message's instance id
 *  @param[in] completion_code - PLDM completion code
 *  @param[in] length - Number of bytes read. This could be less than what the
			 requester asked for.
 *  @param[in,out] msg - Message will be written to this
 *  @return pldm_completion_codes
 *  @note  Caller is responsible for memory alloc and dealloc of param 'msg'
 */
int encode_rw_file_memory_resp(uint8_t instance_id, uint8_t completion_code,
				 uint32_t length, struct pldm_msg *msg);
/** @brief Encode ReadWriteFileIntoMemory commands request data
 * 
 *  @param[in] instance_id - Message's instance id
 *  @param[in,out] msg - Message gets modified as output
 *  @param[in] file_handle - A handle to the file
 *  @param[in] offset -  Offset to the file at which the read should begin
 *  @param[in] length -  Number of bytes to be read
 *  @param[in] address - Memory address where the file content has to be
 *                        written to
 *  @return pldm_completion_codes
 */
int encode_rw_file_memory_req(uint8_t instance_id, struct pldm_msg *msg,
				uint32_t file_handle, uint32_t offset,
				uint32_t length, uint64_t address);
/** @brief Decode ReadWriteFileIntoMemory commands response data
 * 
 *  @param[in,out] msg - Message gets modified as output
 *  @param[in] payload_length - Length of response payload
 *  @param[in,out] completion_code - PLDM completion code
 *  @param[in,out] length - Number of bytes to be read
 *  @return pldm_completion_codes
 */

int decode_rw_file_memory_resp(const uint8_t *msg, size_t payload_length,
				 uint8_t *completion_code, uint32_t *length);

#ifdef __cplusplus
}
#endif

#endif /* FILEIO_H */
