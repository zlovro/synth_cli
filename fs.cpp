//
// Created by lovro on 17/08/2025.
// Copyright (c) 2025 lovro. All rights reserved.
//

#include "fs.h"

#include <fstream>
#include <regex>

extern "C" {
#include <wav/wav.h>
}

nlohmann::json SynthFs::loadJson(const char *pFile) {
    std::ifstream file(pFile);
    if (!file.is_open())
    {
        return nullptr;
    }

    auto pJson = nlohmann::json::parse(file);
    file.close();

    return pJson;
}

void SynthFs::padStream(std::ostream &pOstream, size_t pTo) {
    auto pos = opos(pOstream);
    if (pos % pTo == 0)
    {
        return;
    }

    for (int i = 0; i < pTo - (pos % pTo); ++i)
    {
        pOstream.put(0);
    }
}

size_t SynthFs::writeFileToOfstream(std::ofstream &pOfstream, const char *pFile) {
    size_t p0 = pOfstream.tellp();

    std::ifstream in(pFile, std::ios_base::in | std::ios_base::binary);
    copyStream(pOfstream, in);
    in.close();

    return (size_t) pOfstream.tellp() - p0;
}

size_t SynthFs::writeToFile(const std::filesystem::path &pFile, void *pData, size_t pSize) {
    std::ofstream o(pFile, std::ios_base::out | std::ios_base::binary);
    o.write((str) pData, pSize);
    o.close();
    return o.tellp();
}

size_t writeStreamToFile(const std::filesystem::path &pFile, std::istream &pStream, size_t pOffset, size_t pLength) {
    auto buf = new u8[pLength];

    auto off = pStream.tellg();
    pStream.seekg(pOffset);
    pStream.read((str) buf, pLength);
    pStream.seekg(off);

    auto ret = SynthFs::writeToFile(pFile, buf, pLength);
    delete[] buf;
    return ret;
}

void SynthFs::copyStream(std::ofstream &pOfstream, std::ifstream &pIfstream) {
    while (!pIfstream.eof())
    {
        pOfstream.put(pIfstream.get());
    }
}

std::string bytesToStr(uint64_t bytes) {
    std::string suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char        length   = std::size(suffix);

    int    i        = 0;
    double dblBytes = bytes;

    if (bytes > 1024)
    {
        for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024) dblBytes = bytes / 1024.0;
    }

    char output[64];
    sprintf(output, "%.02lf %s", dblBytes, suffix[i].c_str());
    return output;
}

synthErrno sfsReadBlocks(u8 *pData, u32 pBlkIdx, u32 pBlkCnt) {
    return SERR_SD_GENERIC_ERROR;
}

synthErrno SynthFs::writeImage() {
    solfegeInit();

    std::ofstream sfsImgOut("synth.bin", std::ios_base::out | std::ios_base::binary);

    std::vector<sfsSingleInstrument>  singleInstrumentPool;
    std::vector<sfsInstrumentSample>  samplePool;
    std::vector<std::string>          namePool;
    std::vector<std::vector<u8> >     sampleDataPool;
    std::vector<sfsKeyProximityTable> proximityTablePool;

    std::map<std::string, u16>                                                           mapInstrumentStringIdToNumId;
    std::map<std::filesystem::path, std::map<u8, std::vector<std::pair<u8, size_t> > > > mapProximity;

    u32 currentSampleBlockOffset = 0;
    u32 currentNameOffset        = 0;
    u32 currentInstrumentId      = 0;
    u32 currentSampleId          = 0;

    u32 singleInstrumentCount = 0;
    u32 multiInstrumentCount  = 0;

    for (const auto &instrumentsEnt: std::filesystem::directory_iterator("instruments"))
    {
        if (!instrumentsEnt.is_directory())
        {
            continue;
        }

        const auto &instrumentPath    = instrumentsEnt.path();
        auto        instrumentDirName = instrumentPath.filename();
        auto        instrumentStrId   = instrumentDirName.string();

        mapInstrumentStringIdToNumId[instrumentStrId] = currentInstrumentId;

        #ifdef SYNTH_MULTI
        bool isSingle = config["single"];
        #else
        bool isSingle = true;
        #endif

        if (isSingle)
        {
            singleInstrumentCount++;

            for (const auto &sampleFileEnt: std::filesystem::directory_iterator(instrumentsEnt))
            {
                auto filename = sampleFileEnt.path().filename();
                if (filename.extension().string() != ".wav")
                {
                    continue;
                }

                auto sampleName = filename.replace_extension().string();

                auto underscoreIdx  = sampleName.find('_');
                u8   sampleVelocity = std::stoi(sampleName.substr(underscoreIdx + 1));
                if (sampleVelocity == SFS_INVALID_VELOCITY)
                {
                    return SERR_SFS_INVALID_VELOCITY;
                }

                u8 sampleSemitoneOff = std::stoi(sampleName.substr(0, underscoreIdx));

                if (!mapProximity.contains(instrumentPath))
                {
                    mapProximity[instrumentPath] = std::map<u8, std::vector<std::pair<u8, size_t> > >();
                }

                if (!mapProximity[instrumentPath].contains(sampleSemitoneOff))
                {
                    mapProximity[instrumentPath][sampleSemitoneOff] = std::vector<std::pair<u8, size_t> >();
                }

                mapProximity[instrumentPath][sampleSemitoneOff].push_back(std::pair<u8, size_t>(sampleVelocity, -1));
            }
        }
        else
        {
            multiInstrumentCount++;
        }

        currentInstrumentId++;
    }

    size_t idxCounter = 0;
    for (const auto &key: mapProximity | std::views::keys)
    {
        for (auto &m = mapProximity[key]; auto &[fst, snd]: m)
        {
            auto &vec = snd;
            std::ranges::sort(vec, [](const std::pair<u8, size_t> &left, const std::pair<u8, size_t> &right) {
                return left.first < right.first;
            });

            for (auto &val: vec | std::views::values)
            {
                val = idxCounter++;
            }
            m[fst] = vec;
        }
    }

    printf("Instruments:\n");
    for (auto &instrumentPath: mapProximity | std::ranges::views::keys)
    {
        printf("\t- Instrument %ls\n", instrumentPath.filename().c_str());

        u8 noteRangeStart = 0xFF, noteRangeEnd = 0;

        sfsSingleInstrument instrument = {};

        std::ifstream  instrumentJson(instrumentPath / "instrument.json");
        nlohmann::json config = nlohmann::json::parse(instrumentJson);
        instrumentJson.close();

        sfsSoundType soundType = SFS_SOUND_TYPE_ATTACK;
        if (config["looping"])
        {
            soundType |= SFS_SOUND_TYPE_LOOP;
        }

        instrument.nameStrIndex = namePool.size();
        instrument.soundType    = soundType;
        instrument.release      = config["release"];

        std::string name = config["name"];
        namePool.push_back(name);

        currentNameOffset += name.length() + 1;

        sfsKeyProximityTable table = {};
        table.sampleIdxOrigin      = currentSampleId;
        for (int key = SFS_FIRST_KEY, i = 0; key <= SFS_LAST_KEY; ++key, ++i)
        {
            sfsKeyProximityTableEntryMaster entryMaster = {};

            int delta   = 100;
            int closest = 0;
            for (int semitone: mapProximity[instrumentPath] | std::ranges::views::keys)
            {
                if (abs(semitone - key) < delta)
                {
                    delta   = abs(semitone - key);
                    closest = semitone;
                }
            }

            int j = 0;
            for (const auto &pair: mapProximity[instrumentPath][closest])
            {
                int    velocity = pair.first;
                size_t idx      = pair.second;

                sfsKeyProximityTableEntryVelocity entry = {};

                entry.velocity  = velocity;
                entry.sampleIdx = idx - table.sampleIdxOrigin;

                entryMaster.byVelocity[j++] = entry;
            }

            table.masterEntries[i] = entryMaster;
        }

        proximityTablePool.push_back(table);

        for (auto &pair0: mapProximity[instrumentPath])
        {
            u8   sampleSemitoneOff = pair0.first;
            auto vec               = pair0.second;
            for (auto &pair: vec)
            {
                u8 sampleVelocity = pair.first;

                auto sampleFilenameBase = std::to_string(sampleSemitoneOff) + "_" + std::to_string(sampleVelocity);
                auto sampleFileEnt      = instrumentPath / (sampleFilenameBase + ".wav");
                auto sampleFileJson     = instrumentPath / (sampleFilenameBase + ".json");

                nlohmann::json sampleJson = loadJson(sampleFileJson.generic_string().c_str());

                if (sampleSemitoneOff > noteRangeEnd)
                {
                    noteRangeEnd = sampleSemitoneOff;
                }

                if (sampleSemitoneOff < noteRangeStart)
                {
                    noteRangeStart = sampleSemitoneOff;
                }

                std::ifstream fileStream(sampleFileEnt, std::ios_base::binary | std::ios_base::in);
                std::string   readData;
                u32           sampleRate = 0;

                while (!fileStream.eof())
                {
                    readData += (char) fileStream.get();

                    std::string::size_type i;
                    if (!sampleRate && (i = readData.find("fmt ")) != std::string::npos)
                    {
                        fileStream.seekg(0xC - 4, std::ios_base::cur);
                        fileStream.read((str) &sampleRate, 4);
                        fileStream.seekg(-8 - 4, std::ios_base::cur);

                        if (sampleRate != SFS_SAMPLERATE)
                        {
                            return SERR_SFS_INVALID_SAMPLERATE;
                        }
                    }
                    else if ((i = readData.find("data")) != std::string::npos)
                    {
                        break;
                    }
                }

                u32 dataSize;
                fileStream.read((str) &dataSize, 4);
                u32 sampleLengthSamples = dataSize / 2;

                sfsInstrumentSample sample = {};

                sample.pcmDataLengthSamples = sampleLengthSamples;
                sample.pcmDataBlockOffset   = currentSampleBlockOffset;

                currentSampleBlockOffset += roundUpTo(dataSize, BLOCK_SIZE) / BLOCK_SIZE;

                if (soundType & SFS_SOUND_TYPE_LOOP)
                {
                    sample.loopStart    = (u32) sampleJson["loopStart"].get<int>();
                    sample.loopDuration = (u32) sampleJson["loopDuration"].get<int>();
                }
                else
                {
                    sample.loopStart    = 0;
                    sample.loopDuration = 0;
                }

                sample.velocity       = sampleVelocity;
                sample.pitchSemitones = sampleSemitoneOff;

                auto sampleData = std::vector<u8>(dataSize);
                fileStream.read((str) sampleData.data(), dataSize);

                u32 amp1 = 0, amp2 = 0;

                constexpr int expectedAverageAmplitudeArea = 10000;
                int           averageAmplitudeArea;

                if (sampleLengthSamples > expectedAverageAmplitudeArea * 2)
                {
                    averageAmplitudeArea = expectedAverageAmplitudeArea;
                }
                else
                {
                    averageAmplitudeArea = sampleLengthSamples / 2 - 1;
                }

                for (size_t i = 0; i < averageAmplitudeArea; i++)
                {
                    amp1 += *((u16 *) sampleData.data() + i);
                    amp2 += *((u16 *) sampleData.data() + sampleData.size() / 2 - i - 1);
                }

                sample.startAverageAmplitude = amp1 / averageAmplitudeArea;
                sample.endAverageAmplitude   = amp2 / averageAmplitudeArea;

                samplePool.push_back(sample);
                sampleDataPool.push_back(sampleData);

                currentSampleId++;

                fileStream.close();
            }
        }

        instrument.noteRangeStart = noteRangeStart;
        instrument.noteRangeEnd   = noteRangeEnd;

        singleInstrumentPool.push_back(instrument);
    }

    u32 instrumentCount = singleInstrumentCount + multiInstrumentCount;

    sfsHeader header = {};
    header.magic     = SFS_MAGIC;

    sfsImgOut.seekp(BLOCK_SIZE * 1, std::ios_base::beg);
    printf("\nWriting file: \n");

    // hold behaviours
    {
        printf("\t- Writing hold behaviour data...\n");
        size_t p0 = sfsImgOut.tellp();

        std::vector<std::vector<sfsHoldBehaviour> > holdBehaviours(singleInstrumentCount);

        sfsHoldBehaviour dummyHold = {};
        dummyHold.instrumentId     = SFS_INVALID_INSTRUMENT_ID;

        for (size_t i = 0; i < singleInstrumentCount; i++)
        {
            holdBehaviours[i] = std::vector(1, dummyHold);
        }

        size_t holdBehaviourCount = 0;

        nlohmann::json holdJson = loadJson("instruments/hold.json");
        if (holdJson != nullptr)
        {
            for (auto it = holdJson.begin(); it != holdJson.end(); ++it)
            {
                auto key = it.key();

                auto behavioursJson = holdJson[key];
                if (behavioursJson.size() > holdBehaviourCount)
                {
                    holdBehaviourCount = behavioursJson.size();
                }

                std::vector<sfsHoldBehaviour> behaviourVector;

                for (auto behaviour: behavioursJson)
                {
                    sfsHoldBehaviour behaviourObj = {};

                    behaviourObj.triggerTime    = behaviour["triggerTime"];
                    behaviourObj.maxTriggerTime = behaviour["maxTriggerTime"];
                    behaviourObj.transitionTime = behaviour["transitionTime"];
                    behaviourObj.instrumentId   = mapInstrumentStringIdToNumId[behaviour["instrument"]];

                    behaviourVector.push_back(behaviourObj);
                }

                auto keyRegex = std::regex(key);
                for (const auto &instrumentStrId: mapInstrumentStringIdToNumId | std::views::keys)
                {
                    if (std::regex_match(instrumentStrId, keyRegex))
                    {
                        auto instrumentNumId = mapInstrumentStringIdToNumId[instrumentStrId];
                        for (size_t i = 0; i < behaviourVector.size(); i++)
                        {
                            auto elem = behaviourVector[i];
                            if (elem.instrumentId == instrumentNumId)
                            {
                                behaviourVector.erase(behaviourVector.begin() + i);
                            }
                        }

                        holdBehaviours[instrumentNumId] = behaviourVector;
                    }
                }
            }

            holdBehaviourCount++;

            for (size_t i = 0; i < singleInstrumentCount; i++)
            {
                auto &behaviours = holdBehaviours[i];
                behaviours.insert(behaviours.begin(), dummyHold);

                auto remaining = holdBehaviourCount - behaviours.size();
                for (size_t j = 0; j < remaining; ++j)
                {
                    behaviours.push_back(dummyHold);
                }
            }
        }

        header.holdBehaviorDataStart = obpos(sfsImgOut);

        for (size_t i = 0; i < singleInstrumentCount; i++)
        {
            auto behaviours = holdBehaviours[i];

            for (auto behaviour: behaviours)
            {
                auto ptr = (str) &behaviour;
                sfsImgOut.write(ptr, sizeof(sfsHoldBehaviour));
            }
        }

        padStream(sfsImgOut, BLOCK_SIZE);

        printf("\t- Written %s of hold behaviour data.\n\t- - - - - - - - - - -\n", bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    // pcm data
    {
        printf("\t- Writing PCM data...\n");
        size_t p0 = sfsImgOut.tellp();

        auto pcmStart            = obpos(sfsImgOut);
        header.pcmDataBlockStart = pcmStart;

        for (auto &sample: samplePool)
        {
            sample.pcmDataBlockOffset += pcmStart;
        }

        for (auto pcm: sampleDataPool)
        {
            sfsImgOut.write((str) pcm.data(), pcm.size());
            padStream(sfsImgOut, BLOCK_SIZE);
        }

        printf("\t- Written %s of PCM data.\n\t- - - - - - - - - - -\n", bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    // string LUT
    {
        printf("\t- Writing string LUT data...\n");
        size_t p0 = sfsImgOut.tellp();

        auto lutStart              = obpos(sfsImgOut);
        header.stringLutBlockStart = lutStart;

        u32 offset = 0;
        for (const auto &name: namePool)
        {
            sfsImgOut.write((str) &offset, sizeof(u32));
            offset += name.length() + 1;
        }

        padStream(sfsImgOut, BLOCK_SIZE);

        printf("\t- Written %s of string LUT data.\n\t- - - - - - - - - - -\n",
               bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    // string data
    {
        printf("\t- Writing string data...\n");
        size_t p0 = sfsImgOut.tellp();

        auto strDataStart           = obpos(sfsImgOut);
        header.stringDataBlockStart = strDataStart;

        for (const auto &name: namePool)
        {
            sfsImgOut.write(name.data(), name.length() + 1);
        }

        padStream(sfsImgOut, BLOCK_SIZE);

        printf("\t- Written %s of string data.\n\t- - - - - - - - - - -\n",
               bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    // sfsSingleInstrument data
    {
        printf("\t- Writing instrument info data...\n");
        size_t p0 = sfsImgOut.tellp();

        auto instrumentInfoStart            = obpos(sfsImgOut);
        header.instrumentInfoDataBlockStart = instrumentInfoStart;

        for (const auto &instrument: singleInstrumentPool)
        {
            sfsImgOut.write((str) &instrument, sizeof(sfsSingleInstrument));
        }

        padStream(sfsImgOut, BLOCK_SIZE);

        printf("\t- Written %s of instrument info data.\n\t- - - - - - - - - - -\n",
               bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    // sfsSingleSample data
    {
        printf("\t- Writing sample info data...\n");
        size_t p0 = sfsImgOut.tellp();

        auto sampleInfoStart        = obpos(sfsImgOut);
        header.sampleInfoBlockStart = sampleInfoStart;

        for (const auto &sample: samplePool)
        {
            sfsImgOut.write((str) &sample, sizeof(sfsInstrumentSample));
        }

        padStream(sfsImgOut, BLOCK_SIZE);

        printf("\t- Written %s of sample info data.\n\t- - - - - - - - - - -\n",
               bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    // proximityTableBlockStart data
    {
        printf("\t- Writing proximity tables...\n");
        size_t p0 = sfsImgOut.tellp();

        auto proximityTableStart        = obpos(sfsImgOut);
        header.proximityTableBlockStart = proximityTableStart;

        for (const auto &table: proximityTablePool)
        {
            sfsImgOut.write((str) &table, sizeof(sfsKeyProximityTable));
        }

        padStream(sfsImgOut, BLOCK_SIZE);

        printf("\t- Written %s of proximity tables.\n\t- - - - - - - - - - -\n",
               bytesToStr((size_t) sfsImgOut.tellp() - p0).c_str());
    }

    auto end = sfsImgOut.tellp();

    header.instrumentCount       = instrumentCount;
    header.singleInstrumentCount = singleInstrumentCount;
    header.multiInstrumentCount  = multiInstrumentCount;

    printf("\t- Writing header...\n");

    sfsImgOut.seekp(0, std::ios_base::beg);
    sfsImgOut.write((str) &header, sizeof(header));

    printf("\t- Written %s header.\n\t- - - - - - - - - - -\n", bytesToStr(sfsImgOut.tellp()).c_str());

    printf("\nWritten %s file.\n", bytesToStr(end).c_str());

    sfsImgOut.close();

    return SERR_OK;
}

synthErrno SynthFs::flashImage() {
    system("wmic diskdrive list brief");
    printf("Select physical drive: ");
    fflush(stdout);

    char pDriveBuf[32];
    gets(pDriveBuf);

    char drive = pDriveBuf[0];

    printf("Flashing drive %c. Confirm (yes): ", drive);
    fflush(stdout);

    char input[32];
    gets(input);

    if (input != std::string("yes"))
    {
        printf("Aborting.\n");
        fflush(stdout);

        return SERR_OK;
    }

    printf("Flashing...\n");
    fflush(stdout);

    char cmd[256];
    sprintf(cmd, "physdiskwrite -u -d %c synth.bin", drive);
    printf("Running command: '%s'\n", cmd);
    fflush(stdout);

    return (synthErrno) system(cmd);
}

std::string instrumentNameToStringId(std::string pName) {
    std::string out;
    for (char c: pName)
    {
        if (isupper(c))
        {
            out += tolower(c);
            continue;
        }

        if (c == ' ' || c == '\t' || c == '\n' || c == ':')
        {
            out += '-';
            continue;
        }

        out += c;
    }

    return out;
}

void writeJson(const nlohmann::json &pJson, const std::filesystem::path &pFile) {
    std::ofstream o(pFile);

    o << pJson.dump(4);
    o.close();
}

// synthErrno SynthFs::extractImage() {
//     std::filesystem::path dst            = "extracted";
//     std::filesystem::path fontDst        = dst / "synth-font";
//     std::filesystem::path instrumentsDst = dst / "instruments";
//
//     std::filesystem::create_directories(dst);
//     std::filesystem::create_directories(fontDst);
//     std::filesystem::create_directories(instrumentsDst);
//
//     std::ifstream fileStream("synth.bin", std::ios::binary | std::ios::in);
//
//     sfsHeader header;
//     fileStream.read((char *) &header, sizeof(sfsHeader));
//
//     auto stringLutRead = [&](size_t idx) {
//         auto buf = new char[0x400];
//
//         if (!fileStream.good())
//         {
//             return std::string("NULL");
//         }
//
//         fileStream.seekg(header.stringLutBlockStart * BLOCK_SIZE + idx * sizeof(u32), std::ios::beg);
//         fileStream.read(buf, sizeof(u32));
//
//         fileStream.seekg(header.stringDataBlockStart * BLOCK_SIZE + *(u32 *) buf, std::ios::beg);
//         fileStream.read(buf, 0x400);
//
//         std::string ret = std::string(buf);
//         delete[] buf;
//
//         return ret;
//     };
//
//     auto sampleInfoRead = [&](size_t idx) {
//         sfsInstrumentSample sample;
//
//         fileStream.seekg((u64) header.sampleInfoBlockStart * BLOCK_SIZE + idx * sizeof(sfsInstrumentSample), std::ios::beg);
//         fileStream.read((char *) &sample, sizeof(sfsInstrumentSample));
//
//         return sample;
//     };
//
//     // auto fontBuf = new u8[SFS_FONT_SIZE];
//     // fileStream.seekg(header.fontDataBlockStart * BLOCK_SIZE, std::ios::beg);
//     // fileStream.read((char *) fontBuf, SFS_FONT_SIZE);
//     // delete[] fontBuf;
//     //
//     // writeToFile(fontDst / "font.bin", fontBuf, SFS_FONT_SIZE);
//
//     auto instruments = new sfsSingleInstrument[header.instrumentCount];
//
//     fileStream.seekg(BLOCK_SIZE * header.instrumentInfoDataBlockStart, std::ios::beg);
//     fileStream.read((char *) instruments, header.instrumentCount * sizeof(sfsSingleInstrument));
//
//     for (size_t i = 0; i < header.instrumentCount; i++)
//     {
//         auto instrument    = instruments[i];
//         u16  idx           = instrument.nameStrIndex;
//         auto name          = stringLutRead(idx);
//         auto strId         = instrumentNameToStringId(name);
//         auto instrumentDst = instrumentsDst / strId;
//         std::filesystem::create_directories(instrumentDst);
//
//         printf("Writing instrument %03llu / %03d - '%s'\r", i, header.instrumentCount, strId.c_str());
//
//         nlohmann::json config = {
//             {"single", true},
//             {"name", name},
//             {"soundType", ""},
//             {"fadeForced", (f32) instrument.release},
//         };
//
//         std::string soundType;
//         if (instrument.soundType & SFS_SOUND_TYPE_ATTACK)
//         {
//             soundType += "attack";
//         }
//
//         if (instruments->soundType & SFS_SOUND_TYPE_LOOP)
//         {
//             soundType += " loop";
//         }
//
//         config["soundType"] = soundType;
//         config["loopInfo"]  = nullptr;
//
//         sfsKeyProximityTable table;
//         fileStream.seekg(BLOCK_SIZE * header.proximityTableBlockStart + i * sizeof(sfsKeyProximityTable), std::ios::beg);
//         fileStream.read((char *) &table, sizeof(sfsKeyProximityTable));
//
//         for (size_t j = 0; j < SFS_KEY_COUNT; ++j)
//         {
//             sfsKeyProximityTableEntryVelocity *byVelocity = table.masterEntries[j].byVelocity;
//
//             u8 lastVelocity = 0;
//             for (size_t k = 0; k < SFS_MAX_VELOCITY_COUNT; ++k)
//             {
//                 auto entry    = byVelocity[k];
//                 auto velocity = entry.velocity;
//                 if (velocity < lastVelocity)
//                 {
//                     break;
//                 }
//                 lastVelocity = velocity;
//
//                 printf("\t%d\n", table.sampleIdxOrigin + entry.sampleIdx);
//
//                 auto sample = sampleInfoRead((u32) table.sampleIdxOrigin + entry.sampleIdx);
//
//                 auto                  keyAbs  = SFS_FIRST_KEY + j;
//                 std::filesystem::path wavFile = instrumentDst / (std::to_string(keyAbs) + "_" + std::to_string(velocity) + ".wav");
//
//                 if (!std::filesystem::exists(wavFile))
//                 {
//                     std::ofstream wav(wavFile, std::ios::out | std::ios::binary);
//
//                     auto pcmSize = sample.pcmDataLengthSamples * SAMPLE_SIZE;
//
//                     fileStream.seekg(BLOCK_SIZE * sample.pcmDataBlockOffset, std::ios::beg);
//
//                     auto buf = new u8[pcmSize];
//                     fileStream.read((str) buf, pcmSize);
//
//                     wavWriteFileDefault(wavFile.generic_string().c_str(), buf, pcmSize);
//                 }
//
//                 if (instruments->soundType & SFS_SOUND_TYPE_LOOP)
//                 {
//                     config["loopInfo"]["notes"][std::to_string(keyAbs)] = {
//                         (u32) sample.loopStart, (u16) sample.loopDuration
//                     };
//                 }
//
//                 lastVelocity = velocity;
//             }
//         }
//     }
//
//     // hold behaviours
//     {
//         nlohmann::json holdJson;
//
//         fileStream.seekg(BLOCK_SIZE * header.holdBehaviorDataStart, std::ios::beg);
//
//         for (size_t i = 0; i < header.instrumentCount; i++)
//         {
//             sfsHoldBehaviour hold;
//             fileStream.read((str) &hold, sizeof(sfsHoldBehaviour));
//
//             if (hold.instrumentId == SFS_INVALID_INSTRUMENT_ID)
//             {
//                 continue;
//             }
//
//             auto instrument      = instruments[i];
//             auto name            = stringLutRead(instrument.nameStrIndex);
//             auto instrumentStrId = instrumentNameToStringId(name);
//
//             // casting so json does not treat packed fields as references
//             holdJson[instrumentStrId] = {
//                 {
//                     {"triggerTime", (f32) hold.triggerTime},
//                     {"maxTriggerTime", (f32) hold.maxTriggerTime},
//                     {"transitionTime", (f32) hold.transitionTime},
//                     {"instrument", instrumentStrId},
//                 }
//             };
//
//             writeJson(holdJson, instrumentsDst / instrumentStrId / "hold.json");
//         }
//     }
//
//     // wav data
//     {
//     }
//
//     delete[] instruments;
//     fileStream.close();
//
//     return SERR_OK;
// }
