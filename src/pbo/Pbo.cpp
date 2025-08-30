#include "grad_aff/pbo/Pbo.h"

#ifdef GRAD_AFF_USE_OPENSSL
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif

#include <boost/algorithm/string.hpp>
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

    //pboName = filename;

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
        entry.second->data = readBytes(*is, entry.second->dataSize);
    }

    preHashPos = is->tellg();
    auto nullByte = readBytes(*is, 1);
    hash = readBytes(*is, 20);
}

#ifdef GRAD_AFF_USE_OPENSSL
bool grad_aff::Pbo::checkHash() {
    if (preHashPos == 0)
        readPbo(true);

    is->seekg(0);
    auto rawPboData = readBytes(*is, preHashPos);

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha1();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, rawPboData.data(), rawPboData.size());
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);
    std::vector<uint8_t> calculatedHash(md_value, md_value + md_len);

    return (calculatedHash == hash);
}
#endif

void grad_aff::Pbo::extractPbo(fs::path outPath)
{
    outPath = outPath / pboName;
    for (auto& entry : entries) {

        auto pathWithoutFilename = (outPath / entry.second->filename).remove_filename();

        if (!fs::exists(pathWithoutFilename)) {
            fs::create_directories(pathWithoutFilename);
        }

        std::ofstream ofs(outPath / entry.second->filename, std::ios::binary);
        writeBytes(ofs, entry.second->data);
        ofs.close();
    }
}


void grad_aff::Pbo::extractSingleFile(fs::path entryName, fs::path outPath, bool fullPath)
{
    if (entries.size() == 0) {
        this->readPbo(false);
    }

    for (auto& entry : entries) {
        if (entry.second->filename == entryName) {
            auto writePath = outPath;
            if (fullPath) {
                writePath = outPath / pboName/ entry.second->filename;
            }
            else {
                writePath = outPath / fs::path(entry.second->filename).filename();
            }

            if (entry.second->data.size() == 0) {
                this->readSingleData(entry.second->filename);
            }

            auto pathWithoutFilename = writePath;
            pathWithoutFilename.remove_filename();
            if (!fs::exists(pathWithoutFilename)) {
                fs::create_directories(pathWithoutFilename);
            }

            std::ofstream ofs(writePath, std::ios::binary);
            writeBytes(ofs, entry.second->data);
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

    std::streamoff targetDataOffset = 0;

    for (auto &entry : entries) {
        if (entry.second->filename == searchEntry) {
            is->seekg(targetDataOffset, std::ios::cur);
            entry.second->data = readBytes(*is, entry.second->dataSize);
        }
        else {
            targetDataOffset += entry.second->dataSize;
        }
    }
}

void grad_aff::Pbo::writePbo(fs::path outPath) {

    if (outPath != "" && fs::exists(outPath)) {
        fs::create_directories(outPath);
    }
    std::ofstream ofs(outPath / pboName, std::ios::binary);
    //ofs.rdbuf();
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
    for (auto& entry : entries) {
        writeZeroTerminatedString(ofs, entry.second->filename.string());
        writeBytes<uint32_t>(ofs, entry.second->packingMethod);
        writeBytes<uint32_t>(ofs, entry.second->orginalSize);
        writeBytes<uint32_t>(ofs, entry.second->reserved);
        writeBytes<uint32_t>(ofs, entry.second->timestamp);
        writeBytes<uint32_t>(ofs, entry.second->dataSize);
    }

    for (int i = 0; i < 21; i++) {
        writeBytes(ofs, { 0x00 });
    }

    for (auto& entry : entries) {
        writeBytes(ofs, entry.second->data);
    }
    writeBytes(ofs, { 0x00 });
    ofs.close();
#ifdef GRAD_AFF_USE_OPENSSL
    auto size = is->tellg();
    is->seekg(0);
    auto rawPboData = readBytes(*is, (std::streamsize)size - 1);

    EVP_MD_CTX *mdctx;
    const EVP_MD *md;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    md = EVP_sha1();
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, rawPboData.data(), rawPboData.size());
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);
    std::vector<uint8_t> calculatedHash(md_value, md_value + md_len);
#else
    std::vector<uint8_t> calculatedHash(20, 0);
#endif
    std::ofstream ofsHash(outPath / pboName, std::ios::binary | std::ios::app);
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
