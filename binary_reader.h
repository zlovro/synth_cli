//
// Created by lovro on 12/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#ifndef BINARY_READER_H
#define BINARY_READER_H

#include <filesystem>
#include <fstream>
#include <algorithm>

#include "types.h"

class BinaryReader {
private:
    u8 *buf;
    u64 pos;
    u64 len;

public:
    explicit BinaryReader(const std::filesystem::path &pFile) {
        std::ifstream mFileStream(pFile, std::ios_base::binary | std::ios_base::in);

        pos = 0;

        mFileStream.seekg(0, std::ios_base::beg);
        auto beg = mFileStream.tellg();
        mFileStream.seekg(0, std::ios_base::end);
        auto length = mFileStream.tellg() - beg;
        mFileStream.seekg(0, std::ios_base::beg);

        len = length;
        buf = new u8[length];
        mFileStream.read((char *) buf, length);
        mFileStream.close();
    }

    ~BinaryReader() {
        delete[] buf;
    }

    int get() {
        if (pos >= len)
        {
            return -1;
        }

        return buf[pos++];
    }

    template<typename T>
    T read() {
        auto ret = *(T*)(buf + pos);
        pos += sizeof(T);
        return ret;
    }

    template<typename T>
    T readOff(size_t pOffset) {
        return *(T*)(buf + pOffset);
    }

    void seek(size_t pOffset) {
        pos = pOffset;
    }

    template<typename T>
    s64 find(T pValue, size_t pOffset) {
        return find((u8 *) &pValue, sizeof(T), pOffset);
    }

    s64 find(std::string pStr, size_t pOffset) {
        return find((u8 *) pStr.data(), pStr.size(), pOffset);
    }

    s64 find(const u8 *pValue, u32 pSize, size_t pOffset) {
        auto last = buf + len;
        auto ptr = std::search(buf + pOffset, last, pValue, pValue + pSize);
        if (ptr == last)
        {
            return -1;
        }

        return ptr - buf;
    }
};


#endif //BINARY_READER_H
