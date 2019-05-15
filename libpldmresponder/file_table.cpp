#include "file_table.hpp"

#include <boost/crc.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <phosphor-logging/log.hpp>

namespace pldm
{

namespace filetable
{

using namespace phosphor::logging;

FileTable::FileTable(const std::string& fileTablePath)
{
    std::ifstream jsonFile(fileTablePath);
    if (!jsonFile.is_open())
    {
        log<level::ERR>("File table config file does not exist",
                        entry("FILE=%s", fileTablePath.c_str()));
        return;
    }

    auto data = Json::parse(jsonFile, nullptr, false);
    if (data.is_discarded())
    {
        log<level::ERR>("Parsing config file failed");
        return;
    }

    uint16_t fileNameLength = 0;
    uint32_t fileSize = 0;
    uint32_t traits = 0;
    size_t tableSize = 0;
    auto iter = fileTable.begin();

    // Iterate through each JSON object in the config file
    for (const auto& record : data)
    {
        std::string filepath = record.value(path, "");
        traits = static_cast<uint32_t>(record.value(fileTraits, 0));

        fs::path fsPath(filepath);
        if (!fs::exists(fsPath))
        {
            continue;
        }

        fileNameLength =
            static_cast<uint16_t>(fsPath.filename().string().size());
        fileSize = static_cast<uint32_t>(fs::file_size(fsPath));
        tableSize = fileTable.size();

        fileTable.resize(tableSize + sizeof(handle) + sizeof(fileNameLength) +
                         fileNameLength + sizeof(fileSize) + sizeof(traits));
        iter = fileTable.begin() + tableSize;

        // Populate the file table with the contents of the JSON entry
        std::copy_n(reinterpret_cast<uint8_t*>(&handle), sizeof(handle), iter);
        std::advance(iter, sizeof(handle));

        std::copy_n(reinterpret_cast<uint8_t*>(&fileNameLength),
                    sizeof(fileNameLength), iter);
        std::advance(iter, sizeof(fileNameLength));

        std::copy_n(reinterpret_cast<const uint8_t*>(fsPath.filename().c_str()),
                    fileNameLength, iter);
        std::advance(iter, fileNameLength);

        std::copy_n(reinterpret_cast<uint8_t*>(&fileSize), sizeof(fileSize),
                    iter);
        std::advance(iter, sizeof(fileSize));

        std::copy_n(reinterpret_cast<uint8_t*>(&traits), sizeof(traits), iter);
        std::advance(iter, sizeof(traits));

        // Create the file entry for the JSON entry
        FileEntry entry{};
        entry.handle = handle;
        entry.fsPath = std::move(fsPath);
        entry.traits = traits;

        // Insert the file entries in the map
        tableEntries.emplace(handle, std::move(entry));
        handle++;
    }

    tableSize = fileTable.size();
    // Add pad bytes
    if ((tableSize % 4) != 0)
    {
        padCount = 4 - (tableSize % 4);
        fileTable.resize(tableSize + padCount, 0);
    }

    // Calculate the checksum
    boost::crc_32_type result;
    result.process_bytes(fileTable.data(), fileTable.size());
    checkSum = result.checksum();
}

std::vector<uint8_t> FileTable::getFileAttrTable() const
{
    std::vector<uint8_t> table(fileTable);
    table.resize(fileTable.size() + sizeof(checkSum));
    auto iter = table.begin() + fileTable.size();
    std::copy_n(reinterpret_cast<const uint8_t*>(&checkSum), sizeof(checkSum),
                iter);
    return table;
}

std::tuple<bool, FileEntry> FileTable::getFileEntry(Handle handle) const
{
    FileEntry entry{};
    auto result = tableEntries.find(handle);
    if (result != tableEntries.end())
    {
        return std::make_tuple(true, result->second);
    }
    else
    {
        return std::make_tuple(false, entry);
    }
}

FileTable& getFileTable(const std::string& fileTablePath)
{
    static FileTable table;
    if (table.empty())
    {
        table = std::move(FileTable(fileTablePath));
    }
    return table;
}

} // namespace filetable
} // namespace pldm
