//
// Created by lovro on 22/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#include "fill.h"

#include <fstream>
#include <vector>
#include <map>
#include <ranges>
#include <cmath>
#include <cstring>

#include <synthinf/serrno.h>
#include <wav/wav.h>

namespace fs = std::filesystem;

synthErrno Fill::fill(std::filesystem::path pDir) {
    const int FIRST_NOTE = 24;
    const int LAST_NOTE  = 84;

    std::map<int, std::vector<int> > velocityMap;

    for (auto &file: fs::directory_iterator(pDir))
    {
        auto &path = file.path();
        if (path.extension() != ".wav")
        {
            continue;
        }

        auto fname      = path.filename().replace_extension("").generic_string();
        auto underscore = fname.find('_');
        auto semitones  = std::stoi(fname.substr(0, underscore));
        auto velocity   = std::stoi(fname.substr(underscore + 1));

        velocityMap[semitones].push_back(velocity);
    }

    for (int i = FIRST_NOTE; i <= LAST_NOTE; ++i)
    {
        int closest = 100000;

        for (auto semitones: velocityMap | std::views::keys)
        {
            int delta = abs(i - semitones);
            if (delta < abs(i - closest))
            {
                closest = semitones;
            }
        }

        for (auto velocity: velocityMap[closest])
        {
            // new sample rate = 48000 / factor
            auto factor = std::pow(2.0, (closest - i) / 12.0);

            auto          srcPath = pDir / std::format("{}_{}.wav", closest, velocity);
            std::ifstream srcFile(srcPath);

            auto srcLen = fs::file_size(srcPath);
            auto srcBuf = new char[srcLen];

            srcFile.read(srcBuf, srcLen);

            auto hdr = (wavHeader*) srcBuf;
            auto srcSampleCount = hdr->dataSize / sizeof(s16);
            auto srcSamples = (s16*) (srcBuf + sizeof(wavHeader));

            delete[] srcBuf;
        }
    }

    return SERR_OK;
}
