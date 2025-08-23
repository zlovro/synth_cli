cmake_minimum_required(VERSION 3.31)
project(synth_cli)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "-static -g")

find_package(ZLIB REQUIRED)

include_directories("." include include/synthinf)

add_executable(synth_cli main.cpp
        binary_reader.cpp
        binary_reader.h
        nki_extract.cpp
        nki_extract.h
        include/pugixml/pugixml.cpp
        pcm.cpp
        pcm.h
        include/wav/wav.c
        include/solfege/solfege.c
        include/synthinf/serrno.c
        fs.cpp
        fs.h
        include/tfd/tinyfiledialogs.c
        include/tfd/more_dialogs/tinyfd_moredialogs.c
        fill.cpp
        fill.h)

target_link_libraries(synth_cli stdc++exp)
target_link_libraries(synth_cli ZLIB::ZLIB)
