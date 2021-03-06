#include "config.h"

#include "file_io.hpp"

#include "file_table.hpp"

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

namespace dma
{

/** @struct AspeedXdmaOp
 *
 * Structure representing XDMA operation
 */
struct AspeedXdmaOp
{
    uint64_t hostAddr; //!< the DMA address on the host side, configured by
                       //!< PCI subsystem.
    uint32_t len;      //!< the size of the transfer in bytes, it should be a
                       //!< multiple of 16 bytes
    uint32_t upstream; //!< boolean indicating the direction of the DMA
                       //!< operation, true means a transfer from BMC to host.
};

constexpr auto xdmaDev = "/dev/xdma";

int DMA::transferDataHost(const fs::path& path, uint32_t offset,
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
        log<level::ERR>("Failed to open the XDMA device", entry("RC=%d", rc));
        return rc;
    }

    utils::CustomFD xdmaFd(fd);

    void* vgaMem;
    vgaMem = mmap(nullptr, pageAlignedLength, upstream ? PROT_WRITE : PROT_READ,
                  MAP_SHARED, xdmaFd(), 0);
    if (MAP_FAILED == vgaMem)
    {
        rc = -errno;
        log<level::ERR>("Failed to mmap the XDMA device", entry("RC=%d", rc));
        return rc;
    }

    std::unique_ptr<void, decltype(mmapCleanup)> vgaMemPtr(vgaMem, mmapCleanup);

    if (upstream)
    {
        std::ifstream stream(path.string(), std::ios::in | std::ios::binary);
        stream.seekg(offset);

        // Writing to the VGA memory should be aligned at page boundary,
        // otherwise write data into a buffer aligned at page boundary and
        // then write to the VGA memory.
        std::vector<char> buffer{};
        buffer.resize(pageAlignedLength);
        stream.read(buffer.data(), length);
        memcpy(static_cast<char*>(vgaMemPtr.get()), buffer.data(),
               pageAlignedLength);

        if (static_cast<uint32_t>(stream.gcount()) != length)
        {
            log<level::ERR>("mismatch between number of characters to read and "
                            "the length read",
                            entry("LENGTH=%d", length),
                            entry("COUNT=%d", stream.gcount()));
            return -1;
        }
    }

    AspeedXdmaOp xdmaOp;
    xdmaOp.upstream = upstream ? 1 : 0;
    xdmaOp.hostAddr = address;
    xdmaOp.len = length;

    rc = write(xdmaFd(), &xdmaOp, sizeof(xdmaOp));
    if (rc < 0)
    {
        rc = -errno;
        log<level::ERR>("Failed to execute the DMA operation",
                        entry("RC=%d", rc), entry("UPSTREAM=%d", upstream),
                        entry("ADDRESS=%lld", address),
                        entry("LENGTH=%d", length));
        return rc;
    }

    if (!upstream)
    {
        std::ofstream stream(path.string(),
                             std::ios::in | std::ios::out | std::ios::binary);

        stream.seekp(offset);
        stream.write(static_cast<const char*>(vgaMemPtr.get()), length);
    }

    return 0;
}

} // namespace dma

Response readFileIntoMemory(const uint8_t* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    Response response((sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    decode_rw_file_memory_req(request, payloadLength, &fileHandle, &offset,
                              &length, &address);

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("File handle does not exist in the file table",
                        entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        return response;
    }

    if (offset + length > fileSize)
    {
        length = fileSize - offset;
    }

    if (length % dma::minSize)
    {
        log<level::ERR>("Read length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_rw_file_memory_resp(0, PLDM_READ_FILE_INTO_MEMORY,
                                   PLDM_INVALID_READ_LENGTH, 0, responsePtr);
        return response;
    }

    using namespace dma;
    DMA intf;
    return transferAll<DMA>(&intf, PLDM_READ_FILE_INTO_MEMORY, value.fsPath,
                            offset, length, address, true);
}

Response writeFileFromMemory(const uint8_t* request, size_t payloadLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    Response response(sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES, 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_RW_FILE_MEM_REQ_BYTES)
    {
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_ERROR_INVALID_LENGTH, 0, responsePtr);
        return response;
    }

    decode_rw_file_memory_req(request, payloadLength, &fileHandle, &offset,
                              &length, &address);

    if (length % dma::minSize)
    {
        log<level::ERR>("Write length is not a multiple of DMA minSize",
                        entry("LENGTH=%d", length));
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_WRITE_LENGTH, 0, responsePtr);
        return response;
    }

    using namespace pldm::filetable;
    auto& table = buildFileTable(FILE_TABLE_JSON);
    FileEntry value{};

    try
    {
        value = table.at(fileHandle);
    }
    catch (std::exception& e)
    {
        log<level::ERR>("File handle does not exist in the file table",
                        entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    if (!fs::exists(value.fsPath))
    {
        log<level::ERR>("File does not exist", entry("HANDLE=%d", fileHandle));
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_INVALID_FILE_HANDLE, 0, responsePtr);
        return response;
    }

    auto fileSize = fs::file_size(value.fsPath);
    if (offset >= fileSize)
    {
        log<level::ERR>("Offset exceeds file size", entry("OFFSET=%d", offset),
                        entry("FILE_SIZE=%d", fileSize));
        encode_rw_file_memory_resp(0, PLDM_WRITE_FILE_FROM_MEMORY,
                                   PLDM_DATA_OUT_OF_RANGE, 0, responsePtr);
        return response;
    }

    using namespace dma;
    DMA intf;
    return transferAll<DMA>(&intf, PLDM_WRITE_FILE_FROM_MEMORY, value.fsPath,
                            offset, length, address, false);
}

Response getFileTable(const uint8_t* request, size_t payloadLength)
{
    uint32_t transferHandle = 0;
    uint8_t transferFlag = 0;
    uint8_t tableType = 0;

    Response response(sizeof(pldm_msg_hdr) +
                      PLDM_GET_FILE_TABLE_MIN_RESP_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (payloadLength != PLDM_GET_FILE_TABLE_REQ_BYTES)
    {
        encode_get_file_table_resp(0, PLDM_ERROR_INVALID_LENGTH, 0, 0, nullptr,
                                   0, responsePtr);
        return response;
    }

    auto rc = decode_get_file_table_req(request, payloadLength, &transferHandle,
                                        &transferFlag, &tableType);
    if (rc)
    {
        encode_get_file_table_resp(0, rc, 0, 0, nullptr, 0, responsePtr);
        return response;
    }

    if (tableType != PLDM_FILE_ATTRIBUTE_TABLE)
    {
        encode_get_file_table_resp(0, PLDM_INVALID_FILE_TABLE_TYPE, 0, 0,
                                   nullptr, 0, responsePtr);
        return response;
    }

    using namespace pldm::filetable;
    auto table = buildFileTable(FILE_TABLE_JSON);
    auto attrTable = table();
    response.resize(response.size() + attrTable.size());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());

    if (attrTable.empty())
    {
        encode_get_file_table_resp(0, PLDM_FILE_TABLE_UNAVAILABLE, 0, 0,
                                   nullptr, 0, responsePtr);
        return response;
    }

    encode_get_file_table_resp(0, PLDM_SUCCESS, 0, PLDM_START_AND_END,
                               attrTable.data(), attrTable.size(), responsePtr);
    return response;
}

} // namespace responder
} // namespace pldm
