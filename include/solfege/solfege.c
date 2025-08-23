//
// Created by Made on 29/01/2025.
//

#include "solfege.h"

#include <stdio.h>

u8   gNoteNameToOffsetTable[] = {['C' - 'A'] = 0, ['D' - 'A'] = 2, ['E' - 'A'] = 4, ['F' - 'A'] = 5, ['G' - 'A'] = 7, ['A' - 'A'] = 9, ['B' - 'A'] = 11,};
char gOffsetToNoteNameTable[] = {[0] = 'C', [2] = 'D', [4] = 'E', [5] = 'F', [7] = 'G', [9] = 'A', [11] = 'B'};

void solfegeInit()
{

}

synthErrno solfegeParseNote(str pString, tone* pOutTone)
{
    size_t len       = strlen(pString);
    bool   noteFound = false;
    u8     semitone  = 0;

    for (int i = 0; i < len; ++i)
    {
        char c = pString[i];
        if (c >= 'a' && c <= 'g')
        {
            c -= 'a' - 'A';
        }

        if (c >= '0' && c <= '8')
        {
            semitone += (c - '0') * 12;
        }
        else
        {
            if (!noteFound)
            {
                if (charIsNote(c))
                {
                    semitone  = gNoteNameToOffsetTable[c - 'A'];
                    noteFound = true;
                }
                else
                {
                    return SERR_SOLFEGE_INVALID_NOTE;
                }
            }
            else
            {
                if (charIsSharp(c))
                {
                    semitone++;
                }
                else if (charIsFlat(c))
                {
                    semitone--;
                }
                else
                {
                    return SERR_SOLFEGE_BAD_ACCIDENTAL;
                }
            }
        }
    }

    *pOutTone = toneFromAbsoluteSemitoneOffset(semitone);

    return SERR_OK;
}

synthErrno solfegeSemitoneToStr(str pString, u8 pSemitones, bool pPreferFlats)
{
    u8 mod    = pSemitones % 12;
    u8 octave = pSemitones / 12;

    char noteNameBuf[8];
    bool isNatural = solfegeToneIsNatural(pSemitones);
    if (isNatural)
    {
        sprintf(noteNameBuf, "%c%d", gOffsetToNoteNameTable[mod], octave);
    }
    else
    {
        char suffix  = pPreferFlats ? 'b' : '#';
        char note = pPreferFlats ? gOffsetToNoteNameTable[mod + 1] : gOffsetToNoteNameTable[mod - 1];
        sprintf(noteNameBuf, "%c%c%d", note, suffix, octave);
    }

    sprintf(pString, "%s", noteNameBuf);

    return SERR_OK;
}

synthErrno solfegeToneWithVelocityToStr(str pString, u8 pSemitones, u8 pVelocity, bool pPreferFlats)
{
    char buf[8];
    solfegeSemitoneToStr(buf, pSemitones, pPreferFlats);

    sprintf(pString, "%s_%d", buf, pVelocity);

    return SERR_OK;
}

bool solfegeToneIsNatural(u8 pSemitoneOffset)
{
    u8 mod = pSemitoneOffset % 12;
    return mod == 0 || mod == 2 || mod == 4|| mod == 5 || mod == 7 || mod == 9 || mod == 11;
}
