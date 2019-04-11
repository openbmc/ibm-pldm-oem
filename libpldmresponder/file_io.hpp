#pragma once

#include <stdint.h>
#include <unistd.h>

#include <vector>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

namespace pldm
{

namespace responder
{

/** @struct CustomFD
 *
 *  RAII wrapper for file descriptor.
 */
struct CustomFD
{
    CustomFD(const CustomFD&) = delete;
    CustomFD& operator=(const CustomFD&) = delete;
    CustomFD(CustomFD&&) = delete;
    CustomFD& operator=(CustomFD&&) = delete;

    CustomFD(int fd) : fd(fd)
    {
    }

    ~CustomFD()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }

    int operator()() const
    {
        return fd;
    }

  private:
    int fd = -1;
};

/** @struct AspeedXdmaOp
 *
 * Structure representing XDMA operation
 */
struct AspeedXdmaOp
{
    uint8_t upstream;  //!< boolean indicating the direction of the DMA
                       //!< operation, true means a transfer from BMC to host.
    uint64_t hostAddr; //!< the DMA address on the host side, configured by
                       //!< PCI subsystem.
    uint32_t len;      //!< the size of the transfer in bytes, it should be a
                       //!< multiple of 16 bytes
} __attribute__((packed));

const char* xdmaDev = "/dev/xdma";

/** @brief Handler for ReadFileInto
 *
 *  @param[in] request - pointer to PLDM request message
 *  @param[in] payload_length - length of the message payload
 *  @param[out] response - response message location
 */
void readFileIntoMemory(const uint8_t* request, size_t payloadLength,
                        pldm_msg* response);

} // namespace responder
} // namespace pldm
