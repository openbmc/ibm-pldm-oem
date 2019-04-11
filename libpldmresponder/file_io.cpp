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

int transferDatatoHost(const fs::path& file, uint32_t offset, uint32_t length,
                       uint64_t address)
{
    // Align the length of the memory mapping to the page size.
    static const size_t pageSize = getpagesize();
    uint32_t numPages = length / pageSize;
    uint32_t pageLength = numPages * pageSize;
    if (length > pageLength)
    {
        pageLength += pageSize;
    }

    auto mmapCleanup = [pageLength](void* vgaMem) {
        munmap(vgaMem, pageLength);
    };

    int fd = -1;
    int rc = 0;
    fd = open(dma::xdmaDev, O_RDWR);
    if (fd < 0)
    {
        std::cerr << "Opening the xdma device failed rc = " << rc << "\n";
        return rc;
    }
    auto xdmaFDPtr = std::make_unique<utils::CustomFD>(fd);
    auto& xdmaFD = *(xdmaFDPtr.get());

    void* vgaMem = nullptr;

    vgaMem = mmap(nullptr, pageLength, PROT_WRITE, MAP_SHARED, xdmaFD(), 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        std::cerr << "mmap operation failed rc = " << rc << "\n";
        return rc;
    }
    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    // Populate the VGA memory with the contents of the file
    std::ifstream stream(file.string());
    stream.seekg(offset);
    stream.read(static_cast<char*>(vgaMemPtr.get()), length);

    struct dma::AspeedXdmaOp xdmaOp
    {
    };
    xdmaOp.upstream = true;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    // Initiate the DMA operation
    rc = write(xdmaFD(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        std::cerr << "the dma operation failed rc = " << rc << "\n";
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

    constexpr auto readFilePath = "";
    uint32_t origLength = length;

    fs::path path{readFilePath};
    if (!fs::exists(path))
    {
        std::cerr << "File does not exist\n";
        encode_read_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    if (length < dma::minSize)
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

    while (length > 0)
    {
        if (length > dma::maxSize)
        {
            auto rc =
                dma::transferDatatoHost(path, offset, dma::maxSize, address);
            if (rc < 0)
            {
                encode_read_file_memory_resp(0, PLDM_ERROR, 0, response);
                return;
            }
            offset += dma::maxSize;
            length -= dma::maxSize;
            address += dma::maxSize;
        }
        else
        {
            auto rc = dma::transferDatatoHost(path, offset, length, address);
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

} // namespace pldm
