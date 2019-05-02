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

namespace responder
{

namespace fs = std::filesystem;

namespace dma
{

int DMA::transferDataHost(const fs::path& file, uint32_t offset,
                          uint32_t length, uint64_t address, bool upstream)
{
    static const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageAlignedLength = numPages * pageSize;

    if (length > pageAlignedLength)
    {
        pageAlignedLength += pageSize;
    }

    auto mmapCleanup = [pageAlignedLength](void* vgaMem) {
        munmap(vgaMem, pageAlignedLength);
    };

    int fd = -1;
    int rc = 0;
    fd = open(xdmaDev, O_RDWR);
    if (fd < 0)
    {
        rc = -errno;
        std::cerr << "Failed to open the XDMA device: " << rc << ": "
                  << strerror(errno) << "\n";

        return rc;
    }

    utils::CustomFD xdmaFd(fd);

    void* vgaMem;
    vgaMem = mmap(nullptr, pageAlignedLength, upstream ? PROT_WRITE : PROT_READ,
                  MAP_SHARED, xdmaFd(), 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        std::cerr << "Failed to mmap the XDMA device: " << rc << ": "
                  << strerror(errno) << "\n";

        return rc;
    }

    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    if (upstream)
    {
        std::ifstream stream(file.string());

        stream.seekg(offset);
        stream.read(static_cast<char*>(vgaMemPtr.get()), length);
    }

    AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = upstream ? 1 : 0;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(xdmaFd(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr << "Failed to execute the DMA operation: " << rc << ": "
                  << strerror(errno) << "\n";

        return rc;
    }

    if (!upstream)
    {
        std::ofstream stream(file.string());

        stream.seekp(offset);
        stream.write(static_cast<const char*>(vgaMemPtr.get()), length);
    }

    return 0;
}

} // namespace dma

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

    if (length % dma::minSize)
    {
        std::cerr << "Readlength is not a multiple of 16\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_READ_LENGTH, 0, response);
        return;
    }

    if (!fs::exists(path))
    {
        std::cerr << "File does not exist\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    auto fileSize = fs::file_size(path);
    if (offset + length > fileSize)
    {
        std::cerr << "Data out of range\n";
        encode_rw_file_memory_resp(0, PLDM_DATA_OUT_OF_RANGE, 0, response);
        return;
    }

    using namespace dma;
    DMA intf;
    transferAll<DMA>(&intf, path, offset, length, address, true, response);
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
    if (length % dma::minSize)
    {
        std::cerr << "Writelength is not a multiple of 16\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_WRITE_LENGTH, 0, response);
        return;
    }

    if (!fs::exists(path))
    {
        std::cerr << "File does not exist\n";
        encode_rw_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    using namespace dma;
    DMA intf;
    transferAll<DMA>(&intf, path, offset, length, address, false, response);
}

} // namespace responder
} // namespace pldm
