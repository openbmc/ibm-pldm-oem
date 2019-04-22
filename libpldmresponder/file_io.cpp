#include "file_io.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#include "libpldm/base.h"

namespace pldm
{

namespace fs = std::filesystem;

namespace dma
{

int transferDataHost(const fs::path& file, uint32_t offset, uint32_t length,
                     uint64_t address, bool upstream)
{
    int fd;
    int rc;
    void* vgaMem;
    AspeedXdmaOp xdmaOp;
    static const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageAlignedLength = numPages * pageSize;

    if (length > pageAlignedLength)
    {
        pageAlignedLength += pageSize;
    }

    fd = open(xdmaDev, O_RDWR);
    if (fd < 0)
    {
        rc = -errno;
        std::cerr << "Failed to open the XDMA device: " << rc << ": "
                  << strerror(errno) << "\n";

        return rc;
    }

    vgaMem = mmap(nullptr, pageAlignedLength, upstream ? PROT_WRITE : PROT_READ,
                  MAP_SHARED, fd, 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        std::cerr << "Failed to mmap the XDMA device: " << rc << ": "
                  << strerror(errno) << "\n";

        close(fd);
        return rc;
    }

    if (upstream)
    {
        std::ifstream stream(file.string());

        stream.seekg(offset);
        stream.read((char*)vgaMem, length);
    }

    xdmaOp.upstream = upstream ? 1 : 0;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(fd, &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr << "Failed to execute the DMA operation: " << rc << ": "
                  << strerror(errno) << "\n";

        munmap(vgaMem, pageAlignedLength);
        close(fd);
        return rc;
    }

    if (!upstream)
    {
        std::ofstream stream(file.string());

        stream.seekp(offset);
        stream.write((const char*)vgaMem, length);
    }

    munmap(vgaMem, pageAlignedLength);
    close(fd);

    return 0;
}

} // namespace dma

static void transferAll(fs::path& path, uint32_t offset, uint32_t length,
                        uint64_t address, bool upstream, pldm_msg* response)
{
    uint32_t origLength = length;

    while (length > 0)
    {
        if (length > dma::maxSize)
        {
            auto rc = dma::transferDataHost(path, offset, dma::maxSize, address,
                                            upstream);
            if (rc < 0)
            {
                encode_rw_file_memory_resp(0, PLDM_ERROR, 0, response);
                return;
            }

            offset += dma::maxSize;
            length -= dma::maxSize;
            address += dma::maxSize;
        }
        else
        {
            auto rc =
                dma::transferDataHost(path, offset, length, address, upstream);
            if (rc < 0)
            {
                encode_rw_file_memory_resp(0, PLDM_ERROR, 0, response);
                return;
            }

            encode_rw_file_memory_resp(0, PLDM_SUCCESS, origLength, response);
            return;
        }
    }
}

void readFileIntoMemory(const uint8_t* request, size_t payloadLength,
                        pldm_msg* response)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;
    fs::path path("");

    if (payloadLength != (sizeof(fileHandle) + sizeof(offset) + sizeof(length) +
                          sizeof(address)))
    {
        encode_rw_file_memory_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_rw_file_memory_req(request, payloadLength, &fileHandle, &offset,
                              &length, &address);

    if (!fs::exists(path))
    {
        std::cerr << "File does not exist\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    if (length % dma::minSize)
    {
        std::cerr << "Readlength is not a multiple of 16\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_READ_LENGTH, 0, response);
        return;
    }

    auto fileSize = fs::file_size(path);
    if (offset + length > fileSize)
    {
        std::cerr << "Data out of range\n";
        encode_rw_file_memory_resp(0, PLDM_DATA_OUT_OF_RANGE, 0, response);
        return;
    }

    transferAll(path, offset, length, address, true, response);
}

void writeFileFromMemory(const uint8_t* request, size_t payloadLength,
                         pldm_msg* response)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;
    fs::path path("");

    if (payloadLength != (sizeof(fileHandle) + sizeof(offset) + sizeof(length) +
                          sizeof(address)))
    {
        encode_rw_file_memory_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_rw_file_memory_req(request, payloadLength, &fileHandle, &offset,
                              &length, &address);

    if (!fs::exists(path))
    {
        std::cerr << "File does not exist\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    if (length % dma::minSize)
    {
        std::cerr << "Writelength is not a multiple of 16\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_WRITE_LENGTH, 0, response);
        return;
    }

    transferAll(path, offset, length, address, false, response);
}

} // namespace pldm
