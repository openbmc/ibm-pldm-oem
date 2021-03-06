#include "libpldmresponder/file_io.hpp"
#include "libpldmresponder/file_table.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "libpldm/base.h"
#include "libpldm/file_io.h"

#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define SD_JOURNAL_SUPPRESS_LOCATION

#include <systemd/sd-journal.h>

std::vector<std::string> logs;

extern "C" {

int sd_journal_send(const char* format, ...)
{
    logs.push_back(format);
    return 0;
}

int sd_journal_send_with_location(const char* file, const char* line,
                                  const char* func, const char* format, ...)
{
    logs.push_back(format);
    return 0;
}
}

namespace fs = std::filesystem;
using Json = nlohmann::json;
using namespace pldm::filetable;

class TestFileTable : public testing::Test
{
  public:
    void SetUp() override
    {
        // Create a temporary directory to hold the config file and files to
        // populate the file table.
        char tmppldm[] = "/tmp/pldm_fileio_table.XXXXXX";
        dir = fs::path(mkdtemp(tmppldm));

        // Copy the sample image files to the directory
        fs::copy("./files", dir);

        imageFile = dir / "NVRAM-IMAGE";
        auto jsonObjects = Json::array();
        auto obj = Json::object();
        obj["path"] = imageFile.c_str();
        obj["file_traits"] = 1;

        jsonObjects.push_back(obj);
        obj.clear();
        cksumFile = dir / "NVRAM-IMAGE-CKSUM";
        obj["path"] = cksumFile.c_str();
        obj["file_traits"] = 4;
        jsonObjects.push_back(obj);

        fileTableConfig = dir / "configFile.json";
        std::ofstream file(fileTableConfig.c_str());
        file << std::setw(4) << jsonObjects << std::endl;
    }

    void TearDown() override
    {
        fs::remove_all(dir);
    }

    fs::path dir;
    fs::path imageFile;
    fs::path cksumFile;
    fs::path fileTableConfig;

    // <4 bytes - File handle - 0 (0x00 0x00 0x00 0x00)>,
    // <2 bytes - Filename length - 11 (0x0b 0x00>
    // <11 bytes - Filename - ASCII for NVRAM-IMAGE>
    // <4 bytes - File size - 1024 (0x00 0x04 0x00 0x00)>
    // <4 bytes - File traits - 1 (0x01 0x00 0x00 0x00)>
    // <4 bytes - File handle - 1 (0x01 0x00 0x00 0x00)>,
    // <2 bytes - Filename length - 17 (0x11 0x00>
    // <17 bytes - Filename - ASCII for NVRAM-IMAGE-CKSUM>
    // <4 bytes - File size - 16 (0x0f 0x00 0x00 0x00)>
    // <4 bytes - File traits - 4 (0x04 0x00 0x00 0x00)>
    // No pad bytes added since the length for both the file entries in the
    // table is 56, which is a multiple of 4.
    // <4 bytes - Checksum - 2088303182(0x4e 0xfa 0x78 0x7c)>
    Table attrTable = {
        0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x4e, 0x56, 0x52, 0x41, 0x4d, 0x2d,
        0x49, 0x4d, 0x41, 0x47, 0x45, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x4e, 0x56, 0x52, 0x41, 0x4d,
        0x2d, 0x49, 0x4d, 0x41, 0x47, 0x45, 0x2d, 0x43, 0x4b, 0x53, 0x55, 0x4d,
        0x10, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x4e, 0xfa, 0x78, 0x7c};
};

namespace pldm
{

namespace responder
{

namespace dma
{

class MockDMA
{
  public:
    MOCK_METHOD5(transferDataHost,
                 int(const fs::path& file, uint32_t offset, uint32_t length,
                     uint64_t address, bool upstream));
};

} // namespace dma
} // namespace responder
} // namespace pldm
using namespace pldm::responder;
using ::testing::_;
using ::testing::Return;

TEST(TransferDataHost, GoodPath)
{
    using namespace pldm::responder::dma;

    MockDMA dmaObj;
    fs::path path("");

    // Minimum length of 16 and expect transferDataHost to be called once
    // returns the default value of 0 (the return type of transferDataHost is
    // int, the default value for int is 0)
    uint32_t length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, true)).Times(1);
    auto response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY,
                                         path, 0, length, 0, true);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // maxsize of DMA
    length = maxSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, true)).Times(1);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // length greater than maxsize of DMA
    length = maxSize + minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, maxSize, 0, true)).Times(1);
    EXPECT_CALL(dmaObj, transferDataHost(path, maxSize, minSize, maxSize, true))
        .Times(1);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // length greater than 2*maxsize of DMA
    length = 3 * maxSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, true)).Times(3);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));

    // check for downstream(copy data from host to BMC) parameter
    length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, false)).Times(1);
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, false);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(responsePtr->payload + sizeof(responsePtr->payload[0]),
                        &length, sizeof(length)));
}

TEST(TransferDataHost, BadPath)
{
    using namespace pldm::responder::dma;

    MockDMA dmaObj;
    fs::path path("");

    // Minimum length of 16 and transferDataHost returning a negative errno
    uint32_t length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, _)).WillOnce(Return(-1));
    auto response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY,
                                         path, 0, length, 0, true);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);

    // length greater than maxsize of DMA and transferDataHost returning a
    // negative errno
    length = maxSize + minSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, _)).WillOnce(Return(-1));
    response = transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path,
                                    0, length, 0, true);
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR);
}

TEST(ReadFileIntoMemory, BadPath)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 10;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    // Pass invalid payload length
    auto response = readFileIntoMemory(requestMsg.data(), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_LENGTH);
}

TEST_F(TestFileTable, ReadFileInvalidFileHandle)
{
    // Invalid file handle in the file table
    uint32_t fileHandle = 2;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    using namespace pldm::filetable;
    // Initialise the file table with 2 valid file handles 0 & 1.
    auto& table = buildFileTable(fileTableConfig.c_str());

    auto response = readFileIntoMemory(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_FILE_HANDLE);
    // Clear the file table contents.
    table.clear();
}

TEST_F(TestFileTable, ReadFileInvalidOffset)
{
    uint32_t fileHandle = 0;
    // The file size is 1024, so the offset is invalid
    uint32_t offset = 1024;
    uint32_t length = 0;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    using namespace pldm::filetable;
    auto& table = buildFileTable(fileTableConfig.c_str());

    auto response = readFileIntoMemory(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_DATA_OUT_OF_RANGE);
    // Clear the file table contents.
    table.clear();
}

TEST_F(TestFileTable, ReadFileInvalidLength)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 100;
    // Length should be a multiple of dma min size(16)
    uint32_t length = 10;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    using namespace pldm::filetable;
    auto& table = buildFileTable(fileTableConfig.c_str());

    auto response = readFileIntoMemory(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_READ_LENGTH);
    // Clear the file table contents.
    table.clear();
}

TEST_F(TestFileTable, ReadFileInvalidEffectiveLength)
{
    uint32_t fileHandle = 0;
    // valid offset
    uint32_t offset = 100;
    // length + offset exceeds the size, so effective length is
    // filesize(1024) - offset(100). The effective length is not a multiple of
    // DMA min size(16)
    uint32_t length = 1024;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    using namespace pldm::filetable;
    auto& table = buildFileTable(fileTableConfig.c_str());

    auto response = readFileIntoMemory(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_READ_LENGTH);
    // Clear the file table contents.
    table.clear();
}

TEST(WriteFileFromMemory, BadPath)
{
    uint32_t fileHandle = 0;
    uint32_t offset = 0;
    uint32_t length = 10;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    // Pass invalid payload length
    auto response = writeFileFromMemory(requestMsg.data(), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_LENGTH);

    // The length field is not a multiple of DMA minsize
    response = writeFileFromMemory(requestMsg.data(), requestMsg.size());
    responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_WRITE_LENGTH);
}

TEST_F(TestFileTable, WriteFileInvalidFileHandle)
{
    // Invalid file handle in the file table
    uint32_t fileHandle = 2;
    uint32_t offset = 0;
    uint32_t length = 16;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    using namespace pldm::filetable;
    // Initialise the file table with 2 valid file handles 0 & 1.
    auto& table = buildFileTable(fileTableConfig.c_str());

    auto response = writeFileFromMemory(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_FILE_HANDLE);
    // Clear the file table contents.
    table.clear();
}

TEST_F(TestFileTable, WriteFileInvalidOffset)
{
    uint32_t fileHandle = 0;
    // The file size is 1024, so the offset is invalid
    uint32_t offset = 1024;
    uint32_t length = 16;
    uint64_t address = 0;

    std::array<uint8_t, PLDM_RW_FILE_MEM_REQ_BYTES> requestMsg{};
    memcpy(requestMsg.data(), &fileHandle, sizeof(fileHandle));
    memcpy(requestMsg.data() + sizeof(fileHandle), &offset, sizeof(offset));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset), &length,
           sizeof(length));
    memcpy(requestMsg.data() + sizeof(fileHandle) + sizeof(offset) +
               sizeof(length),
           &address, sizeof(address));

    using namespace pldm::filetable;
    // Initialise the file table with 2 valid file handles 0 & 1.
    auto& table = buildFileTable(TestFileTable::fileTableConfig.c_str());

    auto response = writeFileFromMemory(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_DATA_OUT_OF_RANGE);
    // Clear the file table contents.
    table.clear();
}

TEST(FileTable, ConfigNotExist)
{
    logs.clear();
    FileTable tableObj("");
    EXPECT_EQ(logs.size(), 1);
}

TEST_F(TestFileTable, ValidateFileEntry)
{
    FileTable tableObj(fileTableConfig.c_str());

    // Test file handle 0, the file size is 1K bytes.
    auto value = tableObj.at(0);
    ASSERT_EQ(value.handle, 0);
    ASSERT_EQ(strcmp(value.fsPath.c_str(), imageFile.c_str()), 0);
    ASSERT_EQ(static_cast<uint32_t>(fs::file_size(value.fsPath)), 1024);
    ASSERT_EQ(value.traits.value, 1);
    ASSERT_EQ(true, fs::exists(value.fsPath));

    // Test file handle 1, the file size is 16 bytes
    auto value1 = tableObj.at(1);
    ASSERT_EQ(value1.handle, 1);
    ASSERT_EQ(strcmp(value1.fsPath.c_str(), cksumFile.c_str()), 0);
    ASSERT_EQ(static_cast<uint32_t>(fs::file_size(value1.fsPath)), 16);
    ASSERT_EQ(value1.traits.value, 4);
    ASSERT_EQ(true, fs::exists(value1.fsPath));

    // Test invalid file handle
    ASSERT_THROW(tableObj.at(2), std::out_of_range);
}

TEST_F(TestFileTable, ValidateFileTable)
{
    FileTable tableObj(fileTableConfig.c_str());

    // Validate file attribute table
    auto table = tableObj();
    ASSERT_EQ(true,
              std::equal(attrTable.begin(), attrTable.end(), table.begin()));
}

TEST_F(TestFileTable, GetFileTableCommand)
{
    // Initialise the file table with a valid handle of 0 & 1
    auto& table = buildFileTable(fileTableConfig.c_str());

    uint32_t transferHandle = 0;
    uint8_t opFlag = 0;
    uint8_t type = PLDM_FILE_ATTRIBUTE_TABLE;
    uint32_t nextTransferHandle = 0;
    uint8_t transferFlag = PLDM_START_AND_END;

    std::array<uint8_t, PLDM_GET_FILE_TABLE_REQ_BYTES> requestMsg{};
    auto request =
        reinterpret_cast<pldm_get_file_table_req*>(requestMsg.data());
    request->transfer_handle = transferHandle;
    request->operation_flag = opFlag;
    request->table_type = type;

    auto response = getFileTable(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_SUCCESS);
    size_t offsetSize = sizeof(responsePtr->payload[0]);
    ASSERT_EQ(0, memcmp(responsePtr->payload + offsetSize, &nextTransferHandle,
                        sizeof(nextTransferHandle)));
    offsetSize += sizeof(nextTransferHandle);
    ASSERT_EQ(0, memcmp(responsePtr->payload + offsetSize, &transferFlag,
                        sizeof(transferFlag)));
    offsetSize += sizeof(transferFlag);
    ASSERT_EQ(0, memcmp(responsePtr->payload + offsetSize, attrTable.data(),
                        attrTable.size()));
    table.clear();
}

TEST_F(TestFileTable, GetFileTableCommandReqLengthMismatch)
{
    std::array<uint8_t, PLDM_GET_FILE_TABLE_REQ_BYTES> requestMsg{};

    // Pass invalid command payload length
    auto response = getFileTable(requestMsg.data(), 0);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_LENGTH);
}

TEST_F(TestFileTable, GetFileTableCommandBufferNull)
{
    // Pass null buffer for payload
    auto response = getFileTable(nullptr, PLDM_GET_FILE_TABLE_REQ_BYTES);
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_ERROR_INVALID_DATA);
}

TEST_F(TestFileTable, GetFileTableCommandOEMAttrTable)
{
    uint32_t transferHandle = 0;
    uint8_t opFlag = 0;
    uint8_t type = PLDM_OEM_FILE_ATTRIBUTE_TABLE;

    std::array<uint8_t, PLDM_GET_FILE_TABLE_REQ_BYTES> requestMsg{};
    auto request =
        reinterpret_cast<pldm_get_file_table_req*>(requestMsg.data());
    request->transfer_handle = transferHandle;
    request->operation_flag = opFlag;
    request->table_type = type;

    auto response = getFileTable(requestMsg.data(), requestMsg.size());
    auto responsePtr = reinterpret_cast<pldm_msg*>(response.data());
    ASSERT_EQ(responsePtr->payload[0], PLDM_INVALID_FILE_TABLE_TYPE);
}
