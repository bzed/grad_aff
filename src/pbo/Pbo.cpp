#include "grad_aff/pbo/Pbo.h"

#ifdef GRAD_AFF_USE_OPENSSL
// Use the modern EVP API for hashing, compatible with OpenSSL 3.x
#include <openssl/evp.h>
#endif

#include <boost/algorithm/string.hpp>
#include <algorithm> // For std::replace

namespace ba = boost::algorithm;

grad_aff::Pbo::Pbo(std::string pboFilename) {
    this->is = std::make_shared<std::ifstream>(pboFilename, std::ios::binary);
    this->pboName = ((fs::path)pboFilename).replace_extension("").string();
};

grad_aff::Pbo::Pbo(std::vector<uint8_t> data, std::string pboName) {
    this->is = std::make_shared<std::stringstream>(std::string(data.begin(), data.end()));
    this->pboName = pboName;
}

void grad_aff::Pbo::readPbo(bool withData) {
    is->seekg(0);
    auto initalZero = readBytes(*is, 1);
    if (initalZero[0] != 0) {
        throw std::runtime_error("Invalid file/no inital zero");
    }
    auto magicNumber = readBytes<uint32_t>(*is);
    if (magicNumber != 0x56657273) {
        throw std::runtime_error("Invalid file/magic number");
    }

    auto sixteenZeros = readBytes(*is, 16);
    while (peekBytes<uint8_t>(*is) != 0)
    {
        productEntries.insert({ readZeroTerminatedString(*is), readZeroTerminatedString(*is) });
    }

    readBytes<uint8_t>(*is);

    // Entry
    while (peekBytes<uint16_t>(*is) != 0) {
        auto entry = std::make_shared<Entry>();
        entry->filename = ba::to_lower_copy(readZeroTerminatedString(*is));
        entry->packingMethod = readBytes<uint32_t>(*is);
        entry->orginalSize = readBytes<uint32_t>(*is);
        entry->reserved = readBytes<uint32_t>(*is);
        entry->timestamp = readBytes<uint32_t>(*is);
        entry->dataSize = readBytes<uint32_t>(*is);
        entries.insert({ entry->filename.string(), entry });
    }

    auto nullBytes = readBytes(*is, 21);
    dataPos = is->tellg();

    if (!withData)
        return;

    for (auto& entry : entries) {
        entry.second->data = readEntry(*entry.second);
    }

    preHashPos = is->tellg();
    auto nullByte = readBytes(*is, 1);
    hash = readBytes(*is, 20);
}

std::vector<uint8_t> grad_aff::Pbo::readEntry(const Entry& entry) {
    auto data = readBytes(*is, entry.dataSize);
    if (entry.orginalSize != 0 && entry.orginalSize != entry.dataSize) {
        std::vector<uint8_t> uncompressed;
        if (readLzss(data, uncompressed) == entry.dataSize) {
            return uncompressed;
        }
        else {
            throw std::runtime_error("Couldn't read data");
        }
    }
    else {
        return data;
    }
}

#ifdef GRAD_AFF_USE_OPENSSL
bool grad_aff::Pbo::checkHash() {
    if (preHashPos == 0)
        readPbo(true);

    is->seekg(0);
    auto rawPboData = readBytes(*is, preHashPos);

    // Updated SHA1 implementation using the OpenSSL 3.x EVP API
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX for hash check");
    }

    if (1 != EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL)) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("SHA1 Init failed during hash check!");
    }

    if (1 != EVP_DigestUpdate(mdctx, rawPboData.data(), rawPboData.size())) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("SHA1 Update failed during hash check!");
    }

    std::vector<uint8_t> calculatedHash(EVP_MD_size(EVP_sha1()));
    unsigned int digest_len;

    if (1 != EVP_DigestFinal_ex(mdctx, calculatedHash.data(), &digest_len)) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("SHA1 Final failed during hash check!");
    }

    EVP_MD_CTX_free(mdctx);

    if (digest_len != hash.size()) {
        return false; // Should not happen for SHA1 (20 bytes)
    }

    return (calculatedHash == hash);
}
#endif

void grad_aff::Pbo::extractPbo(fs::path outPath)
{
    for (auto& entryPair : entries) {
        const auto& entry = entryPair.second;

        // Normalize path separators from Windows '\' to the OS-preferred separator.
        std::string normalizedPathStr = entry->filename.string();
        std::replace(normalizedPathStr.begin(), normalizedPathStr.end(), '\\', fs::path::preferred_separator);
        
        fs::path finalOutPath = outPath / normalizedPathStr;
        fs::path parentDirectory = finalOutPath.parent_path();

        // Create the directory structure if it doesn't already exist.
        if (!parentDirectory.empty() && !fs::exists(parentDirectory)) {
            fs::create_directories(parentDirectory);
        }

        std::ofstream ofs(finalOutPath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(entry->data.data()), entry->data.size());
        ofs.close();
    }
}


void grad_aff::Pbo::extractSingleFile(fs::path entryName, fs::path outPath, bool fullPath) 
{
    if (entries.size() == 0) {
        this->readPbo(false);
    }

    // Normalize the input entry name for comparison, as PBO paths are case-insensitive.
    std::string lowerEntryName = ba::to_lower_copy(entryName.string());
    std::replace(lowerEntryName.begin(), lowerEntryName.end(), '\\', '/'); // Internally compare with '/'

    for (auto& entryPair : entries) {
        auto& entry = entryPair.second;
        
        std::string currentEntryFilename = entry->filename.string();
        std::replace(currentEntryFilename.begin(), currentEntryFilename.end(), '\\', '/');

        if (currentEntryFilename == lowerEntryName) {
            fs::path writePath;
            if (fullPath) {
                // Construct the full path using the OS-preferred separator.
                std::string normalizedPathStr = entry->filename.string();
                std::replace(normalizedPathStr.begin(), normalizedPathStr.end(), '\\', fs::path::preferred_separator);
                writePath = outPath / normalizedPathStr;
            }
            else {
                // Just use the filename component.
                writePath = outPath / entry->filename.filename();
            }

            if (entry->data.size() == 0) {
                this->readSingleData(entry->filename);
            }

            auto parentDirectory = writePath.parent_path();
            if (!parentDirectory.empty() && !fs::exists(parentDirectory)) {
                fs::create_directories(parentDirectory);
            }

            std::ofstream ofs(writePath, std::ios::binary);
            writeBytes(ofs, entry->data);
            ofs.close();
            return;
        }
    }
}


void grad_aff::Pbo::readSingleData(fs::path searchEntry) {
    if (entries.size() == 0) {
        this->readPbo(false);
    }
    
    is->seekg(this->dataPos);

    std::streamoff currentOffset = 0;

    for (auto &entryPair : entries) {
        auto& entry = entryPair.second;
        if (entry->filename == searchEntry) {
            is->seekg(this->dataPos); // Go to the start of the data block
            is->seekg(currentOffset, std::ios::cur); // Seek to the specific file's data
            entry->data = readEntry(*entry);
            return; // Found and read the file, so we can exit.
        }
        currentOffset += entry->dataSize;
    }
}

void grad_aff::Pbo::writePbo(fs::path outPath) {

    if (outPath != "" && !fs::exists(outPath)) {
        fs::create_directories(outPath);
    }
    std::ofstream ofs(outPath / (pboName + ".pbo"), std::ios::binary);

    // write magic
    writeBytes(ofs, { 0x00 });
    writeBytes<uint32_t>(ofs, 0x56657273);

    // write zero
    for (int i = 0; i < 16; i++) {
        writeBytes(ofs, { 0 });
    }

    // write header entries
    for (auto& headEntry : productEntries) {
        writeZeroTerminatedString(ofs, headEntry.first);
        writeZeroTerminatedString(ofs, headEntry.second);
    }
    writeBytes<uint8_t>(ofs, 0);

    // Write Header
    for (auto& entryPair : entries) {
        auto& entry = entryPair.second;
        writeZeroTerminatedString(ofs, entry->filename.string());
        writeBytes<uint32_t>(ofs, entry->packingMethod);
        writeBytes<uint32_t>(ofs, entry->orginalSize);
        writeBytes<uint32_t>(ofs, entry->reserved);
        writeBytes<uint32_t>(ofs, entry->timestamp);
        writeBytes<uint32_t>(ofs, entry->dataSize);
    }

    for (int i = 0; i < 21; i++) {
        writeBytes(ofs, { 0x00 });
    }

    for (auto& entryPair : entries) {
        writeBytes(ofs, entryPair.second->data);
    }
    ofs.flush();
    auto preHashPos = ofs.tellp();
    
    writeBytes(ofs, { 0x00 });
    ofs.close();

    // Re-open to read for hashing
    std::ifstream pbo_read_stream(outPath / (pboName + ".pbo"), std::ios::binary);
    std::vector<char> buffer(preHashPos);
    pbo_read_stream.read(buffer.data(), preHashPos);
    pbo_read_stream.close();

#ifdef GRAD_AFF_USE_OPENSSL
    // Updated SHA1 implementation using the OpenSSL 3.x EVP API
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        throw std::runtime_error("Failed to create EVP_MD_CTX for writing PBO");
    }

    if (1 != EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL)) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("SHA1 Init failed during PBO write");
    }

    if (1 != EVP_DigestUpdate(mdctx, buffer.data(), buffer.size())) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("SHA1 Update failed during PBO write");
    }
    
    std::vector<uint8_t> calculatedHash(EVP_MD_size(EVP_sha1()));
    unsigned int digest_len;

    if (1 != EVP_DigestFinal_ex(mdctx, calculatedHash.data(), &digest_len)) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("SHA1 Final failed during PBO write");
    }

    EVP_MD_CTX_free(mdctx);
#else
    std::vector<uint8_t> calculatedHash(20, 0);
#endif
    std::ofstream ofsHash(outPath / (pboName + ".pbo"), std::ios::binary | std::ios::app);
    writeBytes(ofsHash, calculatedHash);
    ofsHash.close();
}

std::vector<uint8_t> grad_aff::Pbo::getEntryData(fs::path entryPath) {
    if (entries.size() == 0)
        readPbo(false);

    if (ba::istarts_with(entryPath.string(), productEntries["prefix"])) {
        entryPath = (fs::path)ba::to_lower_copy(entryPath.string().substr(productEntries["prefix"].size() + 1));
    }

    auto searchEntry = entries.find(entryPath.string());
    if (searchEntry != entries.end()) {
        if (searchEntry->second->data.size() == 0) {
            readSingleData(searchEntry->second->filename);
        }
        return searchEntry->second->data;
    }
    else {
        return {};
    }
}

bool grad_aff::Pbo::hasEntry(fs::path entryPath) {
    if (entries.size() == 0)
        readPbo(false);

    if (ba::istarts_with(entryPath.string(), productEntries["prefix"])) {
        entryPath = (fs::path)ba::to_lower_copy(entryPath.string().substr(productEntries["prefix"].size() + 1));
    }
    return entries.find(entryPath.string()) != entries.end();
}

