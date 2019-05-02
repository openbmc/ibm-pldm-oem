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

	*file_handle = le32toh(*((uint32_t *)msg));
	*offset = le32toh(*((uint32_t *)(msg + sizeof(*file_handle))));
	*length = le32toh(
	    *((uint32_t *)(msg + sizeof(*file_handle) + sizeof(*offset))));
	*address = le64toh(*((uint64_t *)(msg + sizeof(*file_handle) +
					  sizeof(*offset) + sizeof(*length))));

	return PLDM_SUCCESS;
}

int encode_rw_file_memory_resp(uint8_t instance_id, uint8_t command,
			       uint8_t completion_code, uint32_t length,
			       struct pldm_msg *msg)
{
	struct pldm_header_info header = {0};
	int rc = PLDM_SUCCESS;

	uint8_t *payload = msg->payload;
	*payload = completion_code;

	header.msg_type = PLDM_RESPONSE;
	header.instance = instance_id;
	header.pldm_type = PLDM_IBM_OEM_TYPE;
	header.command = command;

	if ((rc = pack_pldm_header(&header, &(msg->hdr))) > PLDM_SUCCESS) {
		return rc;
	}

	if (msg->payload[0] == PLDM_SUCCESS) {
		uint8_t *dst = msg->payload + sizeof(completion_code);
		length = htole32(length);
		memcpy(dst, &length, sizeof(length));
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

	uint8_t *encoded_msg = msg->payload;
	file_handle = htole32(file_handle);
	memcpy(encoded_msg, &file_handle, sizeof(file_handle));
	encoded_msg += sizeof(file_handle);
	offset = htole32(offset);
	memcpy(encoded_msg, &offset, sizeof(offset));
	encoded_msg += sizeof(offset);
	length = htole32(length);
	memcpy(encoded_msg, &length, sizeof(length));
	encoded_msg += sizeof(length);
	address = htole64(address);
	memcpy(encoded_msg, &address, sizeof(address));

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

	*completion_code = msg[0];
	if (*completion_code == PLDM_SUCCESS) {
		*length =
		    le32toh(*((uint32_t *)(msg + sizeof(*completion_code))));
	} else {
		*length = 0;
	}

	return PLDM_SUCCESS;
}
