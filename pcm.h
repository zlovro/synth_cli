//
// Created by lovro on 13/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#ifndef PCM_H
#define PCM_H

#include <vector>

#include "types.h"

void pcmResample(std::vector<s16> pPcmData, int pSampleRateIn, std::vector<s16> &pPcmOut, int pSampleRateOut);

#endif //PCM_H
