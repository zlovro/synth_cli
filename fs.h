//
// Created by lovro on 17/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#ifndef FS_H
#define FS_H

#define DESKTOP

#include <filesystem>
#include <json.hpp>

extern "C" {
#include "sfs/sfs.h"
#include "types.h"
#include "solfege/solfege.h"
}

#define DESKTOP

#define opos(s) ((size_t)s.tellp())
#define ipos(s) ((size_t)s.tellg())
#define obpos(s) (opos(s) / BLOCK_SIZE)
#define ibpos(s) (ipos(s) / BLOCK_SIZE)

class SynthFs {
public:
    static synthErrno     flashImage();
    static synthErrno     extractImage();
    static synthErrno     writeImage(std::filesystem::path pInstrumentsFolder);
    static void           copyStream(std::ofstream &pOfstream, std::ifstream &pIfstream);
    static size_t         writeFileToOfstream(std::ofstream &pOfstream, const char *pFile);
    static size_t         writeToFile(const std::filesystem::path &pFile, void *pData, size_t pSize);
    static void           padStream(std::ostream &pOstream, size_t pTo);
    static nlohmann::json loadJson(const char *pFile);
};

#endif //FS_H
