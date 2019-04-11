#pragma once

#include <stdint.h>
#include <unistd.h>

#include <filesystem>
#include <vector>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

namespace pldm
{

namespace utils
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

} // namespace utils

namespace dma
{

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

constexpr auto xdmaDev = "/dev/xdma";

// The minimum data size of dma transfer
constexpr uint32_t minSize = 16;

// 16MB - 4096B (16773120 bytes) is the maximum data size of DMA transfer
constexpr size_t maxSize = (16 * 1024 * 1024) - 4096;

namespace fs = std::filesystem;

/** @brief API to transfer data from BMC to host using DMA
 *
 *  @param[in] file - pathname of the file from which to DMA data
 *  @param[in] offset - offset in the file
 *  @param[in] length - length of data to read from the file
 *  @param[in] address - dma address on the host side to transfer data
 */
int transferDatatoHost(const fs::path& file, uint32_t offset, uint32_t length,
                       uint64_t address);

} // namespace dma

/** @brief Handler for ReadFileIntoMemory command
 *
 *  @param[in] request - pointer to PLDM request payload
 *  @param[in] payload_length - length of the message payload
 *  @param[out] response - response message location
 */
void readFileIntoMemory(const uint8_t* request, size_t payloadLength,
                        pldm_msg* response);

} // namespace pldm
