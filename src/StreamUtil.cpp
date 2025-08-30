#include "grad_aff/StreamUtil.h"

/*
    Read
*/

template<typename T>
T grad_aff::readBytes(std::istream& i, std::streamoff offset)
{
    size_t pos = i.tellg();
    uint8_t b;
    i.seekg(offset, std::ios::cur);
    T t = readBytes<T>(i);
    i.seekg(pos);
    return t;
}

// byte
template uint8_t grad_aff::readBytes<uint8_t>(std::istream& is, std::streamoff offset);

template<typename T>
T grad_aff::readBytes(std::istream& is) {
    T t = 0;
    is.read(reinterpret_cast<char*>(&t), sizeof(T));
    return t;
}

uint32_t grad_aff::readBytesAsArmaUShort(std::istream& is) {
    uint32_t t = 0;
    is.read(reinterpret_cast<char*>(&t), 3);
    return t;
};
// bool
template bool grad_aff::readBytes<bool>(std::istream& is);
// byte
template uint8_t grad_aff::readBytes<uint8_t>(std::istream& is);
// byte
template int8_t grad_aff::readBytes<int8_t>(std::istream& is);
// ulong
template uint32_t grad_aff::readBytes<uint32_t>(std::istream& is);
// long
template int32_t grad_aff::readBytes<int32_t>(std::istream& is);
// ushort
template uint16_t grad_aff::readBytes<uint16_t>(std::istream& is);
// short
template int16_t grad_aff::readBytes<int16_t>(std::istream& is);
// float
template float_t grad_aff::readBytes<float_t>(std::istream& is);

// https://community.bistudio.com/wiki/raP_File_Format_-_OFP#CompressedInteger
uint32_t grad_aff::readCompressedInteger(std::istream& is) {
    auto val = readBytes<uint8_t>(is);
    auto ret = val;
    while (val & 0x80) {
        val = readBytes<uint8_t>(is);
        ret += (val - 1) * 0x80;
    }
    return ret;
}

template<typename T>
T grad_aff::peekBytes(std::istream& is) {
    auto pos = is.tellg();
    T t = 0;
    is.read(reinterpret_cast<char*>(&t), sizeof(T));
    is.seekg(pos);
    return t;
}
// byte
template uint8_t grad_aff::peekBytes<uint8_t>(std::istream& is);
// ulong
template uint32_t grad_aff::peekBytes<uint32_t>(std::istream& is);
// ushort
template uint16_t grad_aff::peekBytes<uint16_t>(std::istream& is);
// float
template float_t grad_aff::peekBytes<float_t>(std::istream& is);

XYZTriplet grad_aff::readXYZTriplet(std::istream& is) {
    return std::array<float_t, 3> { readBytes<float_t>(is), readBytes<float_t>(is), readBytes<float_t>(is) };
}

TransformMatrix grad_aff::readMatrix(std::istream& is) {
    TransformMatrix matrix = {};
    for (auto i = 0; i < 4; i++) {
        matrix[i] = readXYZTriplet(is);
    }
    return matrix;
}

D3DCOLORVALUE grad_aff::readD3ColorValue(std::istream& is) {
    D3DCOLORVALUE colorValue = {};
    for (auto i = 0; i < 4; i++) {
        colorValue[i] = readBytes<float_t>(is);
    }
    return colorValue;
}

std::string grad_aff::readString(std::istream& is, int count) {
    std::vector<uint8_t> result(count);
    is.read(reinterpret_cast<char*>(&result[0]), count);
    return std::string(result.begin(), result.end());
}

std::vector<uint8_t> grad_aff::readBytes(std::istream& is, std::streamsize length) {

    if (length == 0)
        return {};

    std::vector<uint8_t> result(length);
    is.read(reinterpret_cast<char*>(&result[0]), length);
    return result;
}

std::string grad_aff::readZeroTerminatedString(std::istream& is) {
    std::string result;
    std::getline(is, result, '\0');
    return result;
}

std::chrono::milliseconds grad_aff::readTimestamp(std::istream& is) {
    return std::chrono::milliseconds(std::chrono::duration<long>(readBytes<uint32_t>(is)));
}

std::pair<std::vector<uint8_t>, size_t> grad_aff::readLZOCompressed(std::istream& is, size_t expectedSize) {
    auto retVec = std::vector<uint8_t>(expectedSize);
    auto retCode = Decompress(is, retVec, expectedSize);
    return std::make_pair(retVec, retCode);
}

template <typename T>
std::pair<std::vector<T>, size_t> grad_aff::readLZOCompressed(std::istream& is, size_t expectedSize) {
    if (expectedSize == 0)
        return {};

    auto bVec = readLZOCompressed(is, expectedSize);

    std::vector<T> retVec;
    retVec.reserve(sizeof(T) * expectedSize);

    for (size_t i = 0; i < bVec.first.size(); i += 4) {
        T f;
        memcpy(&f, &bVec.first.data()[i], sizeof(T));
        retVec.push_back(f);
    }
    return std::make_pair(retVec, bVec.second);

}

template std::pair<std::vector<float_t>, size_t> grad_aff::readLZOCompressed(std::istream& is, size_t expectedSize);
template std::pair<std::vector<uint8_t>, size_t> grad_aff::readLZOCompressed(std::istream& is, size_t expectedSize);
template std::pair<std::vector<uint16_t>, size_t> grad_aff::readLZOCompressed(std::istream& is, size_t expectedSize);
template std::pair<std::vector<uint32_t>, size_t> grad_aff::readLZOCompressed(std::istream& is, size_t expectedSize);

std::vector<uint8_t> grad_aff::readCompressedLZOLZSS(std::istream& is, size_t expectedSize, bool useLzo) {
    if (expectedSize == 0)
        return {};

    if (useLzo) {
        return readLZOCompressed<uint8_t>(is, expectedSize).first;
    }
    if (expectedSize < 1024) {
        return readBytes(is, expectedSize);
    }
    return readLzssBlock(is, expectedSize);
}

std::vector<uint8_t> grad_aff::readCompressed(std::istream& is, size_t expectedSize, bool useCompressionFlag) {
    if (expectedSize == 0)
        return {};
    bool flag = expectedSize >= 1024;
    if (useCompressionFlag) {
        flag = readBytes<bool>(is);
    }
    if (!flag) {
        return readBytes(is, expectedSize);
    }
    return readLZOCompressed<uint8_t>(is, expectedSize).first;
}

template<typename T>
std::vector<T> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag) {

    if (expectedSize == 0)
        return {};
    auto n = readBytes<uint32_t>(is);

    auto uncomp = readCompressed(is, n * expectedSize, useCompressionFlag);
    std::vector<T> retVec;
    retVec.reserve(sizeof(T) * expectedSize);

    for (size_t i = 0; i < uncomp.size(); i += 4) {
        T f;
        memcpy(&f, &uncomp.data()[i], sizeof(T));
        retVec.push_back(f);
    }
    return retVec;
}

template std::vector<uint32_t> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag);
template std::vector<uint16_t> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag);
template std::vector<float_t> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag);

template<typename T>
std::vector<T> grad_aff::readCompressedArrayOld(std::istream& is, size_t expectedSize, bool useCompressionFlag) {

    if (expectedSize == 0)
        return {};
    auto n = readBytes<uint32_t>(is);

    auto uncomp = readCompressedLZOLZSS(is, n * expectedSize, useCompressionFlag);
    std::vector<T> retVec;
    retVec.reserve(sizeof(T) * expectedSize);

    for (size_t i = 0; i < uncomp.size(); i += 4) {
        T f;
        memcpy(&f, &uncomp.data()[i], sizeof(T));
        retVec.push_back(f);
    }
    return retVec;
}

template std::vector<uint32_t> grad_aff::readCompressedArrayOld(std::istream& is, size_t expectedSize, bool useCompressionFlag);
template std::vector<uint16_t> grad_aff::readCompressedArrayOld(std::istream& is, size_t expectedSize, bool useCompressionFlag);
template std::vector<float_t> grad_aff::readCompressedArrayOld(std::istream& is, size_t expectedSize, bool useCompressionFlag);

template<typename T>
std::vector<T> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag, size_t arrSize) {
    auto uncomp = readCompressed(is, arrSize * expectedSize, useCompressionFlag);
    std::vector<T> retVec;
    retVec.reserve(sizeof(T) * expectedSize);

    for (size_t i = 0; i < uncomp.size(); i += 4) {
        T f;
        memcpy(&f, &uncomp.data()[i], sizeof(T));
        retVec.push_back(f);
    }
    return retVec;
}
template std::vector<uint32_t> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag, size_t arrSize);
template std::vector<float_t> grad_aff::readCompressedArray(std::istream& is, size_t expectedSize, bool useCompressionFlag, size_t arrSize);

template<typename T>
std::vector<T> grad_aff::readCompressedFillArray(std::istream& is, bool useCompressionFlag) {
    auto n = readBytes<uint32_t>(is);

    auto defaultFill = readBytes<bool>(is);

    std::vector<T> data;
    if (defaultFill) {
        auto fillValue = readBytes<T>(is);

        for (size_t i = 0; i < n; i++)
        {
            data.push_back(defaultFill);
        }
    }
    else {
        data = readCompressedArray<T>(is, n, useCompressionFlag);
    }
    return data;
}

template std::vector<uint32_t> grad_aff::readCompressedFillArray(std::istream& is, bool useCompressionFlag);

std::vector<uint8_t> grad_aff::readLzssBlock(std::istream& is, size_t expectedSize) {
    if (expectedSize < 1024) {
        return readBytes(is, expectedSize);
    }
    std::vector<uint8_t> result(expectedSize);
    readLzssSized(is, result, expectedSize, false);
    return result;
}

/*
    Write
*/

template<typename T>
void grad_aff::writeBytes(std::ostream& ofs, T t) {
    ofs.write(reinterpret_cast<char*>(&t), sizeof(T));
}

void grad_aff::writeBytesAsArmaUShort(std::ostream& ofs, uint32_t t) {
    ofs.write(reinterpret_cast<char*>(&t), 3);
};

// byte
template void grad_aff::writeBytes<uint8_t>(std::ostream & is, uint8_t t);
// ulong
template void grad_aff::writeBytes<uint32_t>(std::ostream& ofs, uint32_t t);
// ushort
template void grad_aff::writeBytes<uint16_t>(std::ostream& ofs, uint16_t t);
// float
template void grad_aff::writeBytes<float_t>(std::ostream& ofs, float_t t);

void grad_aff::writeString(std::ostream& ofs, std::string string) {
    ofs.write(string.data(), string.size());
}

void grad_aff::writeBytes(std::ostream& ofs, std::vector<uint8_t> bytes) {
    ofs.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void grad_aff::writeZeroTerminatedString(std::ostream& ofs, std::string string) {
    writeString(ofs, string);
    ofs.write("\0", 1);
}

void grad_aff::writeTimestamp(std::ostream& ofs, std::chrono::milliseconds milliseconds) {
    writeBytes<uint32_t>(ofs, milliseconds.count());
}

size_t grad_aff::readLzssFile(std::istream& is, std::vector<uint8_t>& out)
{
    std::streampos inSize = 0;
    inSize = is.tellg();
    is.seekg(0, std::ios::end);
    inSize = is.tellg() - inSize;

    is.seekg(0);

    const int slidingWindowSize = 4096;
    const int bestMatch = 18;
    const int threshold = 2;

    std::vector<uint8_t> textBuffer(slidingWindowSize + bestMatch - 1);
    out.reserve(inSize * 4);

    int textBufferIndex = 0, checkSum = 0, flags = 0;
    textBufferIndex = slidingWindowSize - bestMatch;
    while (is.tellg() < (inSize - static_cast<std::streampos>(4)))
    {
        if (((flags >>= 1) & 256) == 0)
        {
            flags = readBytes<uint8_t>(is) | 0xff00;
        }
        if ((flags & 1) != 0)
        {
            auto data = readBytes<uint8_t>(is);
            checkSum += data;

            out.push_back(data);

            textBuffer[textBufferIndex] = data;
            textBufferIndex++; textBufferIndex &= (slidingWindowSize - 1);
        }
        else
        {
            int pos = readBytes<uint8_t>(is);
            int length = readBytes<uint8_t>(is);
            pos |= (length & 0xf0) << 4; length &= 0x0f; length += threshold;

            int bufferPos = textBufferIndex - pos;
            int bufferLength = length + bufferPos;

            for (; bufferPos <= bufferLength; bufferPos++)
            {
                auto data = textBuffer[bufferPos & (slidingWindowSize - 1)];
                checkSum += data;

                out.push_back(data);

                textBuffer[textBufferIndex] = data;
                textBufferIndex++; textBufferIndex &= (slidingWindowSize - 1);
            }
        }
    }

    auto readChecksum = readBytes<int32_t>(is);
    if (checkSum == readChecksum) {
        return inSize;
    }
    else {
        return -1;
    }
}


size_t grad_aff::readLzss(std::vector<uint8_t> in, std::vector<uint8_t>& out)
{
    auto inSize = in.size();
    auto inIndex = 0;

    const int slidingWindowSize = 4096;
    const int bestMatch = 18;
    const int threshold = 2;

    std::vector<uint8_t> textBuffer(slidingWindowSize + bestMatch - 1);
    out.reserve(inSize * 4);

    int textBufferIndex = 0, checkSum = 0, flags = 0;
    textBufferIndex = slidingWindowSize - bestMatch;
    while (inIndex < (inSize - static_cast<std::streampos>(4)))
    {
        if (((flags >>= 1) & 256) == 0)
        {
            flags = in[inIndex] | 0xff00;
            inIndex++;
        }
        if ((flags & 1) != 0)
        {
            auto data = in[inIndex];
            inIndex++;
            checkSum += data;

            out.push_back(data);

            textBuffer[textBufferIndex] = data;
            textBufferIndex++; textBufferIndex &= (slidingWindowSize - 1);
        }
        else
        {
            int pos = in[inIndex];
            inIndex++;
            int length = in[inIndex];
            inIndex++;
            pos |= (length & 0xf0) << 4; length &= 0x0f; length += threshold;

            int bufferPos = textBufferIndex - pos;
            int bufferLength = length + bufferPos;

            for (; bufferPos <= bufferLength; bufferPos++)
            {
                auto data = textBuffer[bufferPos & (slidingWindowSize - 1)];
                checkSum += data;

                out.push_back(data);

                textBuffer[textBufferIndex] = data;
                textBufferIndex++; textBufferIndex &= (slidingWindowSize - 1);
            }
        }
    }

    int readChecksum;
    std::memcpy(&readChecksum, &in[inIndex], 4);

    if (checkSum == readChecksum) {
        return inSize;
    }
    else {
        return -1;
    }
}

size_t grad_aff::readLzssSized(std::istream& is, std::vector<uint8_t>& out, size_t expectedSize, bool useSignedChecksum)
{
    std::vector<uint8_t> arr(4113);
    out.resize(expectedSize);
    if (expectedSize <= 0u)
    {
        return 0u;
    }
    size_t position = is.tellg();
    size_t num = expectedSize;
    int num2 = 0;
    int num3 = 0;
    for (int i = 0; i < 4078; i++)
    {
        arr[i] = ' ';
    }
    int num4 = 4078;
    int num5 = 0;
    while (num > 0u)
    {
        if (((num5 >>= 1) & 256) == 0)
        {
            int num6 = readBytes<uint8_t>(is);
            num5 = (num6 | 65280);
        }
        if ((num5 & 1) != 0)
        {
            int num6 = readBytes<uint8_t>(is);
            if (useSignedChecksum)
            {
                num3 += (int)((int8_t)num6);
            }
            else
            {
                num3 += (int)((uint8_t)num6);
            }
            out[num2++] = (uint8_t)num6;
            num -= 1u;
            arr[num4] = (char)num6;
            num4++;
            num4 &= 4095;
        }
        else
        {
            int i = readBytes<uint8_t>(is);
            int num7 = readBytes<uint8_t>(is);
            i |= (num7 & 240) << 4;
            num7 &= 15;
            num7 += 2;
            int j = num4 - i;
            int num8 = num7 + j;
            if ((long)(num7 + 1) > (long)((size_t)num))
            {
                throw std::runtime_error("LZSS overflow");
            }
            while (j <= num8)
            {
                int num6 = (int)((uint8_t)arr[j & 4095]);
                if (useSignedChecksum)
                {
                    num3 += (int)((int8_t)num6);
                }
                else
                {
                    num3 += (int)((uint8_t)num6);
                }
                out[num2++] = (uint8_t)num6;
                num -= 1u;
                arr[num4] = (char)num6;
                num4++;
                num4 &= 4095;
                j++;
            }
        }
    }
    std::vector<uint8_t> arr2 = readBytes(is, 4);
    //if (BitConverter.ToInt32(array2, 0) != num3)
    //{
    //    //throw new ArgumentException("Checksum mismatch");
    //}
    return (size_t)is.tellg() - position;
}
