//
// Created by lovro on 22/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#ifndef FILL_H
#define FILL_H
#include <filesystem>

#include "serrno.h"

class Fill {
private:

public:
    static synthErrno fill(std::filesystem::path pDir);
};

#endif //FILL_H
