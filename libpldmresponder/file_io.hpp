#pragma once

#include <stdint.h>
#include <unistd.h>

#include <filesystem>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

namespace pldm
{

namespace responder
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

namespace fs = std::filesystem;

namespace dma
{

// The minimum data size of dma transfer in bytes
constexpr uint32_t minSize = 16;

// 16MB - 4096B (16773120 bytes) is the maximum data size of DMA transfer
constexpr size_t maxSize = (16 * 1024 * 1024) - 4096;

/** @brief API to transfer data between BMC and host using DMA
 *
 * @param[in] path     - pathname of the file to transfer data from or to
 * @param[in] offset   - offset in the file
 * @param[in] length   - length of the data to transfer
 * @param[in] address  - DMA address on the host
 * @param[in] upstream - indicates directon of the transfer; true indicates
 *                       transfer to the host
 *
 * @return             - returns 0 on success, negative errno on failure
 */
int transferDataHost(const fs::path& path, uint32_t offset, uint32_t length,
                     uint64_t address, bool upstream);
} // namespace dma

/** @brief Transfer the data between BMC and host using DMA.
 *
 *  There is a max size for each DMA operation, transferAll API abstracts this
 *  and the requested length is broken down into multiple DMA operations if the
 *  length exceed max size.
 *
 * @param[in] command  - PLDM command
 * @param[in] path     - pathname of the file to transfer data from or to
 * @param[in] offset   - offset in the file
 * @param[in] length   - length of the data to transfer
 * @param[in] address  - DMA address on the host
 * @param[in] upstream - indicates direction of the transfer; true indicates
 *                       transfer to the host
 * @param[out] response - response message location
 */
void transferAll(uint8_t command, fs::path& path, uint32_t offset,
                 uint32_t length, uint64_t address, bool upstream,
                 pldm_msg* response);

/** @brief Handler for readFileIntoMemory command
 *
 *  @param[in] request - pointer to PLDM request payload
 *  @param[in] payloadLength - length of the message payload
 *  @param[out] response - response message location
 */
void readFileIntoMemory(const uint8_t* request, size_t payloadLength,
                        pldm_msg* response);

/** @brief Handler for writeFileIntoMemory command
 *
 *  @param[in] request - pointer to PLDM request payload
 *  @param[in] payloadLength - length of the message payload
 *  @param[out] response - response message location
 */
void writeFileFromMemory(const uint8_t* request, size_t payloadLength,
                         pldm_msg* response);

} // namespace responder
} // namespace pldm
