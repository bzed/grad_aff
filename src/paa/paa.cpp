#include "grad_aff/paa/paa.h"

#include <squish.h>
#include <lzo/lzo1x.h>

#include <boost/gil.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>

namespace bg = boost::gil;

#ifdef GRAD_AFF_USE_OIIO
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/string_view.h>

using namespace OIIO;
#endif


grad_aff::Paa::Paa() {
    this->typeOfPax = TypeOfPaX::DXT5;
};


grad_aff::Paa::Paa(std::string pboFilename) {
    this->is = std::make_shared<std::ifstream>(pboFilename, std::ios::binary);
};

grad_aff::Paa::Paa(std::vector<uint8_t> data) {
    this->is = std::make_shared<std::stringstream>(std::string(data.begin(), data.end()));
}

void grad_aff::Paa::readPaa() {

    //std::ifstream ifs(filename, std::ios::binary);

    magicNumber = readBytes<uint16_t>(*is);
    switch (magicNumber)
    {
    case 0xff01:
        this->typeOfPax = TypeOfPaX::DXT1;
        break;
    case 0xff02:
        this->typeOfPax = TypeOfPaX::DXT2;
        break;
    case 0xff03:
        this->typeOfPax = TypeOfPaX::DXT3;
        break;
    case 0xff04:
        this->typeOfPax = TypeOfPaX::DXT4;
        break;
    case 0xff05:
        this->typeOfPax = TypeOfPaX::DXT5;
        break;
    case 0x4444:
        this->typeOfPax = TypeOfPaX::RGBA4444;
        break;
    case 0x1555:
        this->typeOfPax = TypeOfPaX::RGBA5551;
        break;
    case 0x8888:
        this->typeOfPax = TypeOfPaX::RGBA8888;
        break;
    case 0x8080:
        this->typeOfPax = TypeOfPaX::GRAYwAlpha;
        break;
    default:
        throw std::runtime_error("Invalid file/magic number");
        break;
    }

    // Taggs
    while (is->peek() != 0)
    {
        Tagg tagg;
        tagg.signature = readString(*is, 8);
        tagg.dataLength = readBytes<uint32_t>(*is);
        tagg.data = readBytes(*is, tagg.dataLength);
        taggs.push_back(tagg);

        if (tagg.signature == "GGATGALF") {
            hasTransparency = true;
        }
    }

    // TODO
    palette.dataLength = readBytes<uint16_t>(*is);
    if (palette.dataLength > 0) {
        palette.data = readBytes(*is, palette.dataLength);
    }

    // MipMaps
    while (peekBytes<uint16_t>(*is) != 0) {
        MipMap mipmap;
        mipmap.width = readBytes<uint16_t>(*is);
        mipmap.height = readBytes<uint16_t>(*is);
        mipmap.dataLength = readBytesAsArmaUShort(*is);
        mipmap.data = readBytes(*is, mipmap.dataLength);

        // check if top most bit is set, which indicates lzo compression for DXT files
        if ((mipmap.width & 0x8000) != 0) {
            // correct width
            mipmap.width &= 0x7FFF;
            mipmap.lzoCompressed = true;
        }
        else {
            mipmap.lzoCompressed = false;
        }

        if (mipmap.lzoCompressed) {
            if (lzo_init() == LZO_E_OK) {
                auto lzoUncompressed = std::vector<uint8_t>((size_t)mipmap.dataLength * 4 * 100); // TODO
                lzo_uint out_len;

                if (lzo1x_decompress(reinterpret_cast<const uint8_t*>(mipmap.data.data()), mipmap.dataLength, lzoUncompressed.data(), &out_len, NULL) != LZO_E_OK) {
                    throw std::runtime_error("LZO Decompression failed");
                };

                mipmap.data = std::vector<uint8_t>(lzoUncompressed.data(), lzoUncompressed.data() + out_len);
                mipmap.dataLength = out_len;
                mipmap.data.resize(mipmap.dataLength);
            }
            else {
                throw std::runtime_error("LZO Init failed!");
            }
        }

        // decompress
        if (typeOfPax == TypeOfPaX::DXT1) {
            // DXT1 compression ratio in this case is for whatever reason 8:1
            size_t uncompressedSize = (size_t)mipmap.dataLength * 8;
            auto uncompressedData = std::vector<squish::u8>(uncompressedSize);

            squish::DecompressImage(uncompressedData.data(), mipmap.height, mipmap.height, mipmap.data.data(), squish::kDxt1);

            mipmap.dataLength = uncompressedSize;
            mipmap.data = std::vector<uint8_t>(uncompressedData.data(), uncompressedData.data() + uncompressedSize);
        }
        else if (typeOfPax == TypeOfPaX::DXT5) {
            // DXT5 compression ratio is 4:1
            size_t uncompressedSize = (size_t)mipmap.dataLength * 4;
            auto uncompressedData = std::vector<squish::u8>(uncompressedSize);

            squish::DecompressImage(uncompressedData.data(), (int)mipmap.width, (int)mipmap.height, mipmap.data.data(), squish::kDxt5);

            mipmap.dataLength = uncompressedSize;
            mipmap.data = std::vector<uint8_t>(uncompressedData.data(), uncompressedData.data() + uncompressedSize);
        }
        // TODO: other pax

        mipMaps.push_back(mipmap);
    }
}
#ifdef GRAD_AFF_USE_OIIO
void grad_aff::Paa::readImage(fs::path filename) {
    auto inImage = ImageBuf(filename.string());
    inImage.read();
    mipMaps.clear();

    MipMap mipMap;
    mipMap.width = inImage.spec().width;
    mipMap.height = inImage.spec().height;
    mipMap.data.resize((size_t)mipMap.width * (size_t)mipMap.height * 4);
    inImage.get_pixels(ROI(0, mipMap.width, 0, mipMap.height), TypeDesc::UINT8, mipMap.data.data());
    mipMap.dataLength = mipMap.data.size();

    mipMaps.push_back(mipMap);
    calculateMipmapsAndTaggs();
}

void grad_aff::Paa::writeImage(std::string filename, int level) {
    if (level >= mipMaps.size()) {
        std::stringstream exStream;
        exStream << "Level " << level << " exceeds the mipmap count of " << mipMaps.size();
        throw std::out_of_range(exStream.str());
    }

    int width = mipMaps[level].width;
    int height = mipMaps[level].height;

    auto outImage = ImageOutput::create(filename);
    if (!outImage) {
        throw std::runtime_error("Couldn't create output image!");
    }

    ImageSpec imgSpec(width, height, 4, TypeDesc::UINT8);
    outImage->open(filename, imgSpec);
    outImage->write_image(TypeDesc::UINT8, mipMaps[level].data.data());
    outImage->close();

}
#endif

void grad_aff::Paa::calculateMipmapsAndTaggs() {
    auto curWidth = mipMaps[0].width;
    auto curHeight = mipMaps[0].height;

    for (int level = 0; (curHeight < curWidth ? curHeight : curWidth) > 4; level++) {
        auto dataCopy = mipMaps[level == 0 ? 0 : level - 1].data;

        if (level == 0) {
            mipMaps.clear();
        }

        auto v = bg::interleaved_view(curWidth, curHeight, (bg::rgba8_pixel_t*) dataCopy.data(), (size_t)curWidth * 4);

        auto newWidth = curWidth / 2;
        auto newHeight = curHeight / 2;

        auto subimage = bg::rgba8_image_t(newWidth, newHeight);
        auto subView = bg::view(subimage);
        bg::resize_view(v, subView, bg::bilinear_sampler());

        MipMap mipmap;
        mipmap.width = newWidth;
        mipmap.height = newHeight;

        auto it = subView.begin();
        while (it != subView.end()) {
            mipmap.data.push_back((*it)[0]);
            mipmap.data.push_back((*it)[1]);
            mipmap.data.push_back((*it)[2]);
            mipmap.data.push_back((*it)[3]);
            it++;
        }
        mipmap.dataLength = mipmap.data.size();
        mipMaps.push_back(mipmap);

        curWidth = newWidth;
        curHeight = newHeight;
    }

    // Calculate average color
    for (size_t i = 0; i < mipMaps[0].data.size(); i += 4) {
        averageRed += mipMaps[0].data[i];
        averageGreen += mipMaps[0].data[i + 1];
        averageBlue += mipMaps[0].data[i + 2];
        averageAlpha += mipMaps[0].data[i + 3];
    }

    auto pixelCount = mipMaps[0].width * mipMaps[0].height;

    averageRed /= pixelCount;
    averageGreen /= pixelCount;
    averageBlue /= pixelCount;
    averageAlpha /= pixelCount;

    // Write average Color Tagg
    Tagg taggAvg;
    taggAvg.signature = "GGATCGVA";
    taggAvg.data.push_back(averageRed);
    taggAvg.data.push_back(averageGreen);
    taggAvg.data.push_back(averageBlue);
    taggAvg.data.push_back(averageAlpha);
    taggAvg.dataLength = taggAvg.data.size();
    taggs.push_back(taggAvg);

    Tagg taggMax;
    taggMax.signature = "GGATCXAM";
    for (int i = 0; i < 4; i++)
        taggMax.data.push_back(0xFF);
    taggMax.dataLength = taggMax.data.size();
    taggs.push_back(taggMax);


    // Write Transparency Flag Tagg
    if (averageAlpha != 255) {
        hasTransparency = true;
        Tagg taggFlag;
        taggFlag.signature = "GGATGALF";
        taggFlag.data.push_back(0x01);
        for (int i = 0; i < 3; i++)
            taggFlag.data.push_back(0xFF);
        taggFlag.dataLength = taggFlag.data.size();
        taggs.push_back(taggFlag);
    }
}

void grad_aff::Paa::writePaa(std::string filename, TypeOfPaX typeOfPaX) {

    if (mipMaps.size() <= 1)
        calculateMipmapsAndTaggs();

    std::vector<MipMap> encodedMipMaps = mipMaps;

    // Compression
    this->typeOfPax = typeOfPaX;
    if (this->typeOfPax == TypeOfPaX::UNKNOWN) {
        this->typeOfPax = hasTransparency ? TypeOfPaX::DXT5 : TypeOfPaX::DXT1;
    }

    if (typeOfPax == TypeOfPaX::DXT5) {
        for (auto& mipmap : encodedMipMaps) {
            auto compressedDataLength = mipmap.dataLength / 4;
            auto compressedData = std::vector<uint8_t>(compressedDataLength);

            squish::CompressImage(reinterpret_cast<const uint8_t*>(mipmap.data.data()), (int)mipmap.width, (int)mipmap.height, compressedData.data(), squish::kDxt5);

            mipmap.data = compressedData;
            mipmap.dataLength = compressedDataLength;
        }
        magicNumber = 0xff05;
    }
    else if (typeOfPax == TypeOfPaX::DXT1) {
        for (auto& mipmap : encodedMipMaps) {
            auto compressedDataLength = mipmap.dataLength / 8;
            auto compressedData = std::vector<uint8_t>(compressedDataLength);

            squish::CompressImage(reinterpret_cast<const uint8_t*>(mipmap.data.data()), (int)mipmap.width, (int)mipmap.height, compressedData.data(), squish::kDxt1);

            mipmap.data = compressedData;
            mipmap.dataLength = compressedDataLength;
        }
        magicNumber = 0xff01;
    }

    // compress with lzo, if needed
    if (encodedMipMaps[0].width > 128) {
        if (lzo_init() == LZO_E_OK) {

            for (int i = 0; i < encodedMipMaps.size() && encodedMipMaps[i].width > 128; i++) {
                encodedMipMaps[i].lzoCompressed = true;

                size_t out_len = 0;

                std::vector<unsigned char> outputData(encodedMipMaps[i].data.size() * 2);
                std::vector<unsigned char> workMemory(LZO1X_MEM_COMPRESS);

                if (lzo1x_1_compress(reinterpret_cast<const uint8_t*>(encodedMipMaps[i].data.data()), encodedMipMaps[i].dataLength, outputData.data(), &out_len, workMemory.data()) != LZO_E_OK) {
                    throw std::runtime_error("LZO Compression failed");
                }

                encodedMipMaps[i].data = std::vector<uint8_t>(outputData.data(), outputData.data() + out_len);
                encodedMipMaps[i].dataLength = out_len;

                encodedMipMaps[i].width |= 0x8000;
            }
        }
        else {
            throw std::runtime_error("LZO Init failed!");
        }
    }

    Tagg taggOffs;
    taggOffs.signature = "GGATSFFO";


    uint32_t initalOffset = 0;
    initalOffset += 2; // magic

    for (auto tagg : taggs) {
        initalOffset += 8 + 4; // sig + size of length
        initalOffset += 4; // tagg.dataLength;
    }

    initalOffset += 8 + 4 + 16 * 4; // sig + size of length + 16 * 4byte
    initalOffset += 2; // palletteLength
    if (palette.dataLength > 0) {
        // TODO:
    }
    // initalOffset += 8;

    std::vector<char> offsetAsChars(4);
    int counter = 0;

    for (auto& mipmap : encodedMipMaps) {
        offsetAsChars = std::vector<char>(reinterpret_cast<char*>(&initalOffset), reinterpret_cast<char*>(&initalOffset) +4);

        for (int i = 0; i < 4; i++) {
            taggOffs.data.push_back(offsetAsChars[i]);
        }


        initalOffset += mipmap.dataLength + 2 * 2 + 3;
        counter++;
    }
    taggOffs.dataLength = taggOffs.data.size();

    // Write everything
    std::ofstream ofs(filename, std::ios::binary);

    // Write magic
    writeBytes<uint16_t>(ofs, magicNumber);
    for (auto& tagg : taggs) {
        writeString(ofs, tagg.signature);
        writeBytes<uint32_t>(ofs, tagg.dataLength);
        writeBytes(ofs, tagg.data);
    }

    // Write offset Tag
    writeString(ofs, taggOffs.signature);
    writeBytes<uint32_t>(ofs, taggOffs.dataLength);
    writeBytes(ofs, taggOffs.data);

    writeBytes<uint16_t>(ofs, palette.dataLength);
    if (palette.dataLength > 0) {
        // TODO:
    }

    for (auto& mipmap : encodedMipMaps) {
        writeBytes<uint16_t>(ofs, mipmap.width);
        writeBytes<uint16_t>(ofs, mipmap.height);
        writeBytesAsArmaUShort(ofs, mipmap.dataLength);
        writeBytes(ofs, mipmap.data);
    }

    writeBytes<uint16_t>(ofs, 0x00);
    writeBytes<uint16_t>(ofs, 0x00);
    writeBytes<uint16_t>(ofs, 0x00);

    ofs.close();
}

std::vector<uint8_t> grad_aff::Paa::getRawPixelData(uint8_t level)
{
    if (this->mipMaps.size() == 0) {
        return {};
    } else {
        return this->mipMaps[level].data;
    }
};


uint8_t grad_aff::Paa::getRawPixelDataAt(size_t x, size_t y, uint8_t level) {
    if (this->mipMaps.size() == 0) {
        return {};
    }
    else {
        return this->mipMaps[level].data[x + y * mipMaps[level].width];
    }
}

void grad_aff::Paa::setRawPixelData(std::vector<uint8_t> data, uint8_t level) {
    this->mipMaps[level].data = data;
}
void grad_aff::Paa::setRawPixelDataAt(size_t x, size_t y, uint8_t pixelData, uint8_t level) {
    this->mipMaps[level].data[x + y * mipMaps[level].width] = pixelData;
}
