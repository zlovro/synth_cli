//
// Created by lovro on 13/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#include <print>
#include <format>

#include <json.hpp>

#include "nki_extract.h"
#include "binary_reader.h"
#include "pcm.h"
#include "types.h"
#include "pugixml/pugixml.hpp"

extern "C" {
#include <wav/wav.h>
#include <zlib.h>
#include "solfege/solfege.h"
}

#define WAV_RIFF 0x46464952
#define WAV_data 0x61746164

bool nkiExtract(std::filesystem::path pPath, std::filesystem::path pOutputFolder) {
    std::filesystem::create_directories(pOutputFolder);

    auto reader = BinaryReader(pPath);
    auto magic  = reader.read<u32>();
    if (magic != 0x7FA89012)
    {
        std::print("File {} is not monolith. Please extract the samples (WAV/NCW) to a separate folder.", pPath.generic_string());
        return false;
    }

    std::vector<std::vector<s16> > pcmDatas;

    u32    wavSize    = 0;
    s64    offset     = 0;
    size_t wavCounter = 0;
    while (true)
    {
        auto tmpOffset = reader.find((u32) WAV_MAGIC_RIFF, offset + 1);
        if (tmpOffset == -1)
        {
            break;
        }

        wavSize = reader.readOff<u32>(tmpOffset + 4);
        offset  = tmpOffset;

        auto off            = reader.find((u32) WAV_MAGIC_FMT, offset);
        auto chan           = reader.readOff<u16>(off + 10);
        auto sampleRate     = reader.readOff<u32>(off + 12);
        auto bytesPerSample = reader.readOff<u16>(off + 22) / 8;
        auto bitsPerSample  = bytesPerSample * 8;

        off = reader.find((u32) WAV_MAGIC_DATA, offset) + 4;
        reader.seek(off);

        auto dataSize = reader.read<u32>();

        std::vector<s16> pcm;
        for (int i = 0; i < dataSize / (bytesPerSample * chan); ++i)
        {
            f64 sum = 0;
            for (int j = 0; j < chan; ++j)
            {
                s64 x = 0;
                for (int k = 0; k < bytesPerSample; ++k)
                {
                    x |= reader.read<u8>() << (k * 8);
                }

                x -= (x & 1 << (bitsPerSample - 1)) << 1;
                sum += (f64) x;
            }

            sum /= (f64) (1 << bitsPerSample);
            sum /= (f64) chan;

            s16 val = (s16) (sum * 32768);
            pcm.push_back(val);
        }

        std::vector<s16> out;
        pcmResample(pcm, sampleRate, out, 48000);

        pcmDatas.push_back(out);
    }

    auto zlibHeader = std::string("\x0E\x00\x00\x78\x01");
    auto zlibOffset = reader.find(zlibHeader, offset + wavSize) + 3;
    if (zlibOffset == 2)
    {
        return false;
    }

    reader.seek(zlibOffset);

    std::vector<u8> src;
    while (true)
    {
        auto b = reader.get();
        if (b == -1)
        {
            break;
        }

        src.push_back(b);
    }

    uLongf dstLen = 0x20000;
    auto   dst    = new u8[dstLen];
    uLongf srcLen = src.size();
    uncompress2(dst, &dstLen, src.data(), &srcLen);

    pugi::xml_document xml;
    xml.load_buffer(dst, dstLen);

    auto reverbEnabled = false;
    f32  reverbPreDelay, reverbRoomSize, reverbColor, reverbFilter;

    auto root        = xml.document_element();
    auto program     = root.child("Programs").first_child();
    auto programName = std::string(program.attribute("name").as_string());
    if (std::string::size_type idx; (idx = programName.find('-')) != std::string::npos)
    {
        programName = programName.substr(idx + 1);
    }

    int  release = 48000 * 0.75;
    auto group   = program.child("Groups").first_child();
    if (!group.empty())
    {
        auto intModulators = group.child("IntModulators");
        if (!intModulators.empty())
        {
            auto adsr = intModulators.first_child().find_child_by_attribute("Envelope", "type", "ahdsr");
            if (!adsr.empty())
            {
                release = adsr.find_child_by_attribute("name", "release").attribute("value").as_float();
                release = 48000 * 1000 / release;
            }
        }
    }

    for (auto kEffect: program.child("ProgramSendFX"))
    {
        auto lastChild = kEffect.last_child();
        auto name      = std::string(lastChild.name());
        if (name == "Reverb")
        {
            reverbEnabled  = true;
            reverbPreDelay = lastChild.find_child_by_attribute("name", "preDelay").attribute("value").as_float();
            reverbRoomSize = lastChild.find_child_by_attribute("name", "roomsize").attribute("value").as_float();
            reverbColor    = lastChild.find_child_by_attribute("name", "color").attribute("value").as_float();
            reverbFilter   = lastChild.find_child_by_attribute("name", "filter").attribute("value").as_float();
        }
    }

    auto looping = false;
    for (auto zone: program.child("Zones"))
    {
        auto params = zone.child("Parameters");
        auto sample = zone.child("Sample");

        auto sampleRate       = sample.find_child_by_attribute("name", "sampleRate").attribute("value").as_int();
        auto sampleShiftRatio = 48000.0 / sampleRate;

        auto velocity  = params.find_child_by_attribute("name", "highVelocity").attribute("value").as_int();
        velocity = (int)(velocity * (255.0 / 127.0));

        auto semitones = params.find_child_by_attribute("name", "rootKey").attribute("value").as_int();

        // char str[15];
        // solfegeToneWithVelocityToStr(str, semitones, velocity, true);
        // auto filenameBase = std::string(str);

        auto filenameBase = std::to_string(semitones) + "_" + std::to_string(velocity);

        int loopStart = 0, loopDuration = 0;

        auto loops = zone.child("Loops");
        if (!loops.children().empty())
        {
            looping = true;

            auto loop    = loops.first_child();
            loopStart    = loop.find_child_by_attribute("name", "loopStart").attribute("value").as_int();
            loopDuration = loop.find_child_by_attribute("name", "loopLength").attribute("value").as_int();

            loopStart    = (int) round(loopStart * sampleShiftRatio);
            loopDuration = (int) round(loopDuration * sampleShiftRatio);
        }

        nlohmann::ordered_json sampleJson;
        sampleJson["loopStart"]    = loopStart;
        sampleJson["loopDuration"] = loopDuration;

        auto jsonOut = std::ofstream(pOutputFolder / (filenameBase + ".json"));
        jsonOut << sampleJson.dump(4);
        jsonOut.close();

        auto pcmData = pcmDatas[sample.find_child_by_attribute("name", "uniqueID").attribute("value").as_int()];
        wavWriteFile((pOutputFolder / (filenameBase + ".wav")).generic_string().c_str(), 16, 1, 48000, (u8 *) pcmData.data(), pcmData.size() * sizeof(short));
    }

    nlohmann::ordered_json instrumentJson;

    instrumentJson["name"]    = programName;
    instrumentJson["looping"] = looping;
    instrumentJson["release"] = release;

    instrumentJson["reverb"]         = reverbEnabled;
    instrumentJson["reverbRoomSize"] = reverbRoomSize;
    instrumentJson["reverbColor"]    = reverbColor;
    instrumentJson["reverbFilter"]   = reverbFilter;
    instrumentJson["reverbPreDelay"] = reverbPreDelay;

    instrumentJson["src"] = pPath.generic_string();

    auto jsonOut = std::ofstream(pOutputFolder / "instrument.json");
    jsonOut << instrumentJson.dump(4);
    jsonOut.close();

    return true;
}
