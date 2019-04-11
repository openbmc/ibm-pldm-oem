#pragma once

#include <stdint.h>

#include <vector>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

namespace pldm
{

using Type = uint8_t;

namespace responder
{

/** @struct AspeedXdmaOp
 *
 * Structure representing XDMA operation
 *
 * upstream : boolean indicating the direction of the DMA operation; upstream
 * means a transfer from the BMC to the host.
 *
 * hostAddr : the DMA address on the host side, typically configured by PCI
 * subsystem
 *
 * len : the size of the transfer in bytes, it should be a multiple of 16 bytes
 *
 */
struct AspeedXdmaOp
{
    uint8_t upstream;
    uint64_t hostAddr;
    uint32_t len;
} __attribute__((packed));

const char* xdmaDev = "/dev/xdma";

/** @brief Handler for ReadFileInto
 *
 *  @param[in] request - Request message payload
 *  @param[out] response - Response message written here
 */
void readFileIntoMemory(const uint8_t* request, size_t payload_length,
                        pldm_msg* response);

} // namespace responder
} // namespace pldm
