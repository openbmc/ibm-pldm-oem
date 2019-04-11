#include "file_io.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <phosphor-logging/log.hpp>

#include "libpldm/base.h"

namespace pldm
{

namespace responder
{

namespace fs = std::filesystem;
using namespace phosphor::logging;

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
        log<level::ERR>("Opening the xdma device failed", entry("RC=%d", rc));
        return rc;
    }
    auto xdmaFDPtr = std::make_unique<utils::CustomFD>(fd);
    auto& xdmaFD = *(xdmaFDPtr.get());

    void* vgaMem = nullptr;

    vgaMem = mmap(nullptr, pageLength, PROT_WRITE, MAP_SHARED, xdmaFD(), 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        log<level::ERR>("mmap operation failed", entry("RC=%d", rc));
        return rc;
    }
    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    // Populate the VGA memory with the contents of the file
    std::ifstream stream(file.string());
    stream.seekg(offset);
    stream.read(static_cast<char*>(vgaMemPtr.get()), length);
    if (stream.gcount() != length)
    {
        log<level::ERR>(
            "mismatch between number of characters to read and the length read",
            entry("LENGTH=%d", length), entry("COUNT=%d", stream.gcount()));
        return -1;
    }

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
        log<level::ERR>("the dma operation failed", entry("RC=%d", rc),
                        entry("UPSTREAM=%d", xdmaOp.upstream),
                        entry("ADDRESS=%lld", address),
                        entry("LENGTH=%d", length));
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

    if (payloadLength != PLDM_READ_FILE_MEM_REQ_BYTES)
    {
        encode_read_file_memory_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, response);
        return;
    }

    decode_read_file_memory_req(request, payloadLength, &fileHandle, &offset,
                                &length, &address);

    constexpr auto readFilePath = "";

    fs::path path{readFilePath};
    if (!fs::exists(path))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_read_file_memory_resp(0, PLDM_INVALID_FILE_HANDLE, 0, response);
        return;
    }

    auto fileSize = fs::file_size(path);

    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_read_file_memory_resp(0, PLDM_DATA_OUT_OF_RANGE, 0, response);
        return;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    if (length % dma::minSize)
    {
        log<level::ERR>("Readlength is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_read_file_memory_resp(0, PLDM_INVALID_READ_LENGTH, 0, response);
        return;
    }

    uint32_t origLength = length;

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

} // namespace responder
} // namespace pldm
