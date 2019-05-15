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
};

class TestFileAttrTable : public testing::Test
{
  public:
    void SetUp() override
    {
        // Create a temporary directory to hold the config file and files to
        // populate the file table.
        char tmppldm[] = "/tmp/pldm_fileio_table.XXXXXX";
        dir = fs::path(mkdtemp(tmppldm));
        std::cerr << dir.c_str() << std::endl;

        // Copy the sample image files to the directory
        fs::copy_file("./files/NVRAM-IMAGE", dir / "NVRAM-IMAGE");

        imageFile = dir / "NVRAM-IMAGE";
        auto jsonObjects = Json::array();
        auto obj = Json::object();
        obj["path"] = imageFile.c_str();
        obj["file_traits"] = 1;
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
    fs::path fileTableConfig;
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
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES>
        responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    fs::path path("");

    // Minimum length of 16 and expect transferDataHost to be called once
    // returns the default value of 0 (the return type of transferDataHost is
    // int, the default value for int is 0)
    uint32_t length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, true)).Times(1);
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, true, response);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));

    // maxsize of DMA
    length = maxSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, true)).Times(1);
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, true, response);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));

    // length greater than maxsize of DMA
    length = maxSize + minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, maxSize, 0, true)).Times(1);
    EXPECT_CALL(dmaObj, transferDataHost(path, maxSize, minSize, maxSize, true))
        .Times(1);
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, true, response);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));

    // length greater than 2*maxsize of DMA
    length = 3 * maxSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, true)).Times(3);
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, true, response);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));

    // check for downstream(copy data from host to BMC) parameter
    length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(path, 0, length, 0, false)).Times(1);
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, false, response);
    ASSERT_EQ(response->payload[0], PLDM_SUCCESS);
    ASSERT_EQ(0, memcmp(response->payload + sizeof(response->payload[0]),
                        &length, sizeof(length)));
}

TEST(TransferDataHost, BadPath)
{
    using namespace pldm::responder::dma;

    MockDMA dmaObj;
    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES>
        responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());
    fs::path path("");

    // Minimum length of 16 and transferDataHost returning a negative errno
    uint32_t length = minSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, _)).WillOnce(Return(-1));
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, true, response);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);

    // length greater than maxsize of DMA and transferDataHost returning a
    // negative errno
    length = maxSize + minSize;
    EXPECT_CALL(dmaObj, transferDataHost(_, _, _, _, _)).WillOnce(Return(-1));
    transferAll<MockDMA>(&dmaObj, PLDM_READ_FILE_INTO_MEMORY, path, 0, length,
                         0, true, response);
    ASSERT_EQ(response->payload[0], PLDM_ERROR);
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

    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES>
        responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // Pass invalid payload length
    readFileIntoMemory(requestMsg.data(), 0, response);
    ASSERT_EQ(response->payload[0], PLDM_ERROR_INVALID_LENGTH);
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

    std::array<uint8_t, sizeof(pldm_msg_hdr) + PLDM_RW_FILE_MEM_RESP_BYTES>
        responseMsg{};
    pldm_msg* response = reinterpret_cast<pldm_msg*>(responseMsg.data());

    // Pass invalid payload length
    writeFileFromMemory(requestMsg.data(), 0, response);
    ASSERT_EQ(response->payload[0], PLDM_ERROR_INVALID_LENGTH);

    // The length field is not a multiple of DMA minsize
    writeFileFromMemory(requestMsg.data(), requestMsg.size(), response);
    ASSERT_EQ(response->payload[0], PLDM_INVALID_WRITE_LENGTH);
}

TEST(FileTable, configNotExist)
{
    logs.clear();
    FileTable tableObj("");
    EXPECT_EQ(logs.size(), 1);
}

TEST_F(TestFileTable, testFileEntry)
{
    FileTable tableObj(fileTableConfig.c_str());

    // Test file handle 0, the file size is 1K bytes.
    auto [rc, value] = tableObj.getFileEntry(0);
    ASSERT_EQ(rc, true);
    ASSERT_EQ(value.handle, 0);
    ASSERT_EQ(strcmp(value.fsPath.c_str(), imageFile.c_str()), 0);
    ASSERT_EQ(static_cast<uint32_t>(fs::file_size(value.fsPath)), 1024);
    ASSERT_EQ(value.traits, 1);

    // Test file handle 1, the file size is 16 bytes
    auto [rc1, value1] = tableObj.getFileEntry(1);
    ASSERT_EQ(rc1, true);
    ASSERT_EQ(value1.handle, 1);
    ASSERT_EQ(strcmp(value1.fsPath.c_str(), cksumFile.c_str()), 0);
    ASSERT_EQ(static_cast<uint32_t>(fs::file_size(value1.fsPath)), 16);
    ASSERT_EQ(value1.traits, 4);

    // Test invalid file handle
    auto [rc2, value2] = tableObj.getFileEntry(2);
    ASSERT_EQ(rc2, false);
}

TEST_F(TestFileAttrTable, testFileTable)
{
    FileTable tableObj(fileTableConfig.c_str());

    // Test file handle 0
    auto [rc, value] = tableObj.getFileEntry(0);
    ASSERT_EQ(rc, true);
    ASSERT_EQ(value.handle, 0);
    ASSERT_EQ(strcmp(value.fsPath.c_str(), imageFile.c_str()), 0);
    ASSERT_EQ(static_cast<uint32_t>(fs::file_size(value.fsPath)), 1024);
    ASSERT_EQ(value.traits, 1);

    // <4 bytes - File handle - 0 (0x00 0x00 0x00 0x00)>,
    // <2 bytes - Filename length - 11 (0x0b 0x00>
    // <11 bytes - Filename - ASCII for NVRAM-IMAGE>
    // <4 bytes - File size - 1024 (0x00 0x04 0x00 0x00)>
    // <4 bytes - File traits - 1 (0x01 0x00 0x00 0x00)>
    // <3 bytes - Padding (0x00 0x00 0x00)>
    // <4 bytes - Checksum - 1818411840(0x40 0xc3 0x62 0x6c)>
    std::vector<uint8_t> attrTable = {
        0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x4e, 0x56, 0x52, 0x41, 0x4d,
        0x2d, 0x49, 0x4d, 0x41, 0x47, 0x45, 0x00, 0x04, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xc3, 0x62, 0x6c};

    // Validate file attribute table
    auto table = tableObj.getFileAttrTable();
    ASSERT_EQ(true,
              std::equal(attrTable.begin(), attrTable.end(), table.begin()));
}
