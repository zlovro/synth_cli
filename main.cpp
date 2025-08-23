#include <iostream>
#include <print>
#include <format>

#include "argparse.hpp"
#include "binary_reader.h"
#include "fs.h"
#include "nki_extract.h"
#include <tfd/tinyfiledialogs.h>

#include "fill.h"
#include "zlib.h"
#include "types.h"

int main(int pArgc, char *pArgv[]) {
    synthErrno ret = SERR_CMD_INVALID_ARGUMENT;

    argparse::ArgumentParser program("synth-cli");

    argparse::ArgumentParser subExtractNki("nkiex");
    subExtractNki.add_argument("-i", "--input-file");
    subExtractNki.add_argument("-l", "--file-list");
    subExtractNki.add_argument("-o", "--output-folder");

    argparse::ArgumentParser subMkImg("mkimg");
    argparse::ArgumentParser subFlash("flash");

    argparse::ArgumentParser subFill("fill");
    subFill.add_argument("-i", "--instrument-folder");

    program.add_subparser(subExtractNki);
    program.add_subparser(subMkImg);
    program.add_subparser(subFlash);
    program.add_subparser(subFill);

    try
    {
        program.parse_args(pArgc, pArgv);

        if (program.is_subcommand_used(subExtractNki))
        {
            ret = SERR_OK;

            if (subExtractNki.is_used("--input-file"))
            {
                auto inputFile    = subExtractNki.get("--input-file");
                auto outputFolder = subExtractNki.get("--output-folder");

                nkiExtract(inputFile, outputFolder);
            }
            else if (subExtractNki.is_used("--file-list"))
            {
                auto outputFolder = subExtractNki.get("--output-folder");
            }
        }
        else if (program.is_subcommand_used(subMkImg))
        {
            ret = SynthFs::writeImage();
        }
        else if (program.is_subcommand_used(subFlash))
        {
            ret = SynthFs::flashImage();
        }
        else if (program.is_subcommand_used(subFill))
        {
            ret = Fill::fill(subFill.get("--instrument-folder"));
        }
    } catch (const std::exception &err)
    {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    return ret;
}
