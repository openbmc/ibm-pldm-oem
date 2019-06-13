#include "file_io.h"
#include <endian.h>
#include <string.h>

int decode_rw_file_memory_req(const uint8_t *msg, size_t payload_length,
			      uint32_t *file_handle, uint32_t *offset,
			      uint32_t *length, uint64_t *address)
{
	if (msg == NULL || file_handle == NULL || offset == NULL ||
	    length == NULL || address == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_RW_FILE_MEM_REQ_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}

	struct PLDM_ReadWrite_File_Memory_Request *src =
	    (struct PLDM_ReadWrite_File_Memory_Request *)msg;

	*file_handle = le32toh(src->file_handle);
	*offset = le32toh(src->offset);
	*length = le32toh(src->length);
	*address = le64toh(src->address);

	return PLDM_SUCCESS;
}

int encode_rw_file_memory_resp(uint8_t instance_id, uint8_t command,
			       uint8_t completion_code, uint32_t length,
			       struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_IBM_OEM_TYPE;
	header.command = command;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct PLDM_ReadWrite_File_Memory_Response *dst =
	    (struct PLDM_ReadWrite_File_Memory_Response *)msg->payload;
	dst->completion_code = completion_code;
	if (dst->completion_code == PLDM_SUCCESS) {
		length = htole32(length);
		dst->length = length;
	}

	return PLDM_SUCCESS;
}

int encode_rw_file_memory_req(uint8_t instance_id, uint8_t command,
			      struct pldm_msg *msg, uint32_t file_handle,
			      uint32_t offset, uint32_t length,
			      uint64_t address)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;
	if (msg == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}
	header.msg_type = PLDM_REQUEST;
	header.instance = instance_id;
	header.pldm_type = PLDM_IBM_OEM_TYPE;
	header.command = command;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	struct PLDM_ReadWrite_File_Memory_Request *encoded_msg =
	    (struct PLDM_ReadWrite_File_Memory_Request *)msg->payload;
	file_handle = htole32(file_handle);
	(encoded_msg->file_handle) = file_handle;
	offset = htole32(offset);
	(encoded_msg->offset) = offset;
	length = htole32(length);
	(encoded_msg->length) = length;
	address = htole64(address);
	(encoded_msg->address) = address;
	return PLDM_SUCCESS;
}

int decode_rw_file_memory_resp(const uint8_t *msg, size_t payload_length,
			       uint8_t *completion_code, uint32_t *length)
{
	if (msg == NULL || length == NULL || completion_code == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (payload_length != PLDM_RW_FILE_MEM_RESP_BYTES) {
		return PLDM_ERROR_INVALID_LENGTH;
	}
	struct PLDM_ReadWrite_File_Memory_Response *src =
	    (struct PLDM_ReadWrite_File_Memory_Response *)msg;
	*completion_code = src->completion_code;
	if (*completion_code == PLDM_SUCCESS) {
		*length = le32toh(src->length);
	} else {
		*length = 0;
	}

	return PLDM_SUCCESS;
}
