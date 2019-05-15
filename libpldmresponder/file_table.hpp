#pragma once

#include <stdint.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>

#include "libpldm/base.h"

namespace pldm
{

namespace filetable
{

namespace fs = std::filesystem;
using Handle = uint32_t;
using Json = nlohmann::json;

static constexpr auto path = "path";
static constexpr auto fileTraits = "file_traits";

/** @struct FileEntry
 *
 *  Data structure for storing information regarding the files supported by
 *  PLDM File I/O. The file handle is used to uniquely identify the file. The
 *  traits provide information whether the file is Read only, Read/Write and
 *  preserved across firmware upgrades.
 */
struct FileEntry
{
    uint32_t handle; //!< File handle
    fs::path fsPath; //!< File path
    uint32_t traits; //!< File traits
};

/** @class FileTable
 *
 *  FileTable class encapsulates the data related to files supported by PLDM
 *  File I/O and provides interfaces to lookup files information based on the
 *  file handle and extract the file attribute table. The file attribute table
 *  comprises of metadata for files. Metadata includes the file handle, file
 *  name, current file size and file traits.
 */
class FileTable
{
  public:
    FileTable(const std::string& fileTablePath);
    FileTable() = default;
    ~FileTable() = default;
    FileTable(const FileTable&) = default;
    FileTable& operator=(const FileTable&) = default;
    FileTable(FileTable&&) = default;
    FileTable& operator=(FileTable&&) = default;

    /** @brief Get the file attribute table
     *
     * @return contents of the file attribute table
     */
    std::vector<uint8_t> getFileAttrTable() const;

    /** @brief Return the FileEntry if the file handle is valid
     *
     * @param[in] handle - file handle
     *
     * @return a tuple, a bool indicating whether there is an entry for the
     *         file handle and the FileEntry if the bool is true.
     */
    std::tuple<bool, FileEntry> getFileEntry(Handle handle) const;

    /** @brief Check is file attribute table is empty
     *
     * @return bool - true if file attribute table is empty, false otherwise.
     */
    bool isEmpty() const
    {
        return fileTable.empty();
    }

    /** @brief Clear the file table contents
     *
     */
    void clear()
    {
        tableEntries.clear();
        fileTable.clear();
        padCount = 0;
        checkSum = 0;
        handle = 0;
    }

  private:
    /** @brief handle to FileEntry mappings for lookups based on file handle */
    std::unordered_map<Handle, FileEntry> tableEntries;

    /** @brief file attribute table including the pad bytes, except the checksum
     */
    std::vector<uint8_t> fileTable;

    /** @brief the pad count of the file attribute table, the number of pad
     * bytes is between 0 and 3 */
    uint8_t padCount = 0;

    /** @brief the checksum of the file attribute table */
    uint32_t checkSum = 0;

    /** @brief file handle value */
    Handle handle = 0;
};

/** @brief Build the file attribute table if not already built using the
 *         file table config.
 *
 *  @param[in] fileTablePath - path of the file table config
 *
 *  @return FileTable& - Reference to instance of file table
 */

FileTable& getFileTable(const std::string& fileTablePath);

} // namespace filetable
} // namespace pldm
