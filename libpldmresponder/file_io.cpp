#include "file_io.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <filesystem>
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

int transferDatatoHost(const fs::path& file, const uint32_t offset,
                       const uint32_t length, const uint64_t address)
{
    // Align the length of the memory mapping to the page size.
    const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageLength = numPages * pageSize;
    if (length > pageLength)
        pageLength += pageSize;

    int rc = 0;
    int fd = -1;
    void* vgaMem = nullptr;
    std::ifstream stream(file.string());

    auto cleanupFunc = [pageLength](void* vgaMem) {
        munmap(vgaMem, pageLength);
    };

    fd = open(xdmaDev, O_RDWR);
    if (fd < 0)
    {
        std::cerr << "Opening the xdma device failed\n";
        return -ENODEV;
    }

    auto xdmaFDPtr = std::make_unique<CustomFD>(fd);
    auto& xdmaFD = *(xdmaFDPtr.get());

    vgaMem = mmap(NULL, pageLength, PROT_WRITE, MAP_SHARED, xdmaFD(), 0);
    if (!vgaMem)
    {
        std::cerr << "mmap operation failed\n";
        return -ENOMEM;
    }
    std::unique_ptr<void, decltype(cleanupFunc)> ctxPtr(vgaMem, cleanupFunc);

    stream.seekg(offset);
    stream.read(static_cast<char*>(vgaMem), length);

    struct AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = true;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(xdmaFD(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr << "Writing the DMA operation to the xdma device failed\n";
        return rc;
    }

    return rc;
}

void readFileIntoMemory(const uint8_t* request, size_t payloadLength,
                        pldm_msg* response)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    if (payloadLength != (sizeof(fileHandle) + sizeof(offset) + sizeof(length) +
                          sizeof(address)))
    {
        encode_read_file_memory_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_read_file_memory_req(request, payloadLength, &fileHandle, &offset,
                                &length, &address);

    // Hardcoding the file name till the GetFileTable infrastructure is in
    // place.
    constexpr auto readFilePath = "/tmp/readfile";
    uint32_t origLength = length;

    fs::path path{readFilePath};
    if (!fs::exists(path))
    {
        std::cerr << "File does not exist\n";
        encode_read_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    constexpr uint32_t minLength = 16;
    if (length < minLength)
    {
        std::cerr << "Readlength is less than 16\n";
        encode_read_file_memory_resp(0, PLDM_INVALID_READ_LENGTH, 0, response);
        return;
    }

    auto fileSize = fs::file_size(path);
    if (offset + length > fileSize)
    {
        std::cerr << "Data out of range\n";
        encode_read_file_memory_resp(0, PLDM_DATA_OUT_OF_RANGE, 0, response);
        return;
    }

    // There is a restriction on the maximum size to 16MB - 4096B. This should
    // be made into a configurable parameter 16773120
    constexpr size_t maxDMASize = (16 * 1024 * 1024) - 4096;

    while (length > 0)
    {
        if (length > maxDMASize)
        {
            auto rc = transferDatatoHost(path, offset, maxDMASize, address);
            if (rc < 0)
            {
                encode_read_file_memory_resp(0, PLDM_ERROR, 0, response);
                return;
            }
            offset += maxDMASize;
            length -= maxDMASize;
            address += maxDMASize;
        }
        else
        {
            auto rc = transferDatatoHost(path, offset, length, address);
            if (rc < 0)
            {
                encode_read_file_memory_resp(0, PLDM_ERROR, 0, response);
                return;
            }
            encode_read_file_memory_resp(0, PLDM_SUCCESS, origLength, response);
            return;
        }
    }
}

} // namespace responder
} // namespace pldm
