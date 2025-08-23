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

#include "json.hpp"

extern "C" {
#include <synthinf/serrno.h>
#include <wav/wav.h>
}

namespace fs = std::filesystem;
using namespace nlohmann;

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

        if (closest == i)
        {
            continue;
        }

        for (auto velocity: velocityMap[closest])
        {
            auto          srcPath = pDir / std::format("{}_{}.wav", closest, velocity);
            std::ifstream srcFile(srcPath);

            auto srcLen = fs::file_size(srcPath);
            auto srcBuf = new char[srcLen];

            srcFile.read(srcBuf, srcLen);

            auto hdr            = (wavHeader *) srcBuf;
            auto srcSampleCount = hdr->dataSize / sizeof(s16);
            auto srcSamples     = (s16 *) (srcBuf + sizeof(wavHeader));

            auto factor         = std::pow(2.0, (closest - i) / 12.0);
            auto dstSampleCount = (size_t) round(srcSampleCount * factor);
            auto dstSamples     = new s16[dstSampleCount];

            auto t    = 0.0;
            auto step = (double) srcSampleCount / (double) (dstSampleCount - 1);
            for (int j = 0; j < dstSampleCount; ++j)
            {
                auto floor = (size_t) t;
                auto a     = srcSamples[floor];
                auto b     = srcSamples[floor + 1];
                auto c     = t - floor;

                auto x        = a + (b - a) * c;
                dstSamples[j] = (s16) round(x);

                t += step;
            }

            auto dstPath = pDir / std::format("{}_{}.wav", i, velocity);
            wavWriteFileDefault(dstPath.generic_string().c_str(), (u8*)dstSamples, dstSampleCount * sizeof(s16));

            std::ifstream srcJsonFile(pDir / std::format("{}_{}.json", closest, velocity));

            ordered_json srcJson;
            srcJsonFile >> srcJson;

            // used instead of variable factor because of rounding errors
            auto dstToSrcRatio = (double) dstSampleCount / (double) srcSampleCount;

            srcJson["loopStart"]    = (int) round(srcJson["loopStart"].get<int>() * dstToSrcRatio);
            srcJson["loopDuration"] = (int) round(srcJson["loopDuration"].get<int>() * dstToSrcRatio);

            std::ofstream dstJsonFile(pDir / std::format("{}_{}.json", i, velocity));
            dstJsonFile << srcJson.dump(4);

            delete[] srcBuf;
            delete[] dstSamples;
        }
    }

    return SERR_OK;
}
