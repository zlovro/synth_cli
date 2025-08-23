//
// Created by lovro on 13/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#include "pcm.h"

f64 lerp(f64 a, f64 b, f64 t) {
    return a + (b - a) * t;
}

void pcmResample(std::vector<s16> pPcmData, int pSampleRateIn, std::vector<s16> &pPcmOut, int pSampleRateOut) {
    auto ratio          = (f64) pSampleRateOut / (f64) pSampleRateIn;
    auto inSampleCount  = pPcmData.size();
    auto outSampleCount = (size_t) ((f64) inSampleCount * ratio);
    auto step           = (f64) inSampleCount / (f64) (outSampleCount + 1);

    auto t = 0.0;
    for (int i = 0; i < outSampleCount; ++i)
    {
        auto floor    = (int) t;
        auto ceil     = floor + 1;
        auto mantissa = t - floor;

        s32 a = pPcmData[floor];
        s32 b = pPcmData[ceil];

        f64 value = lerp(a, b, mantissa);
        pPcmOut.push_back((s16) value);

        t += step;
    }
}
