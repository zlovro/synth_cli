//
// Created by Made on 29/01/2025.
//

#ifndef SOLFEGE_H
#define SOLFEGE_H

#include <serrno.h>
#include <string.h>
#include <math.h>
#include <types.h>

typedef pstruct
{
    f32 frequency;
    u16 semitoneOffset;
    u8  padding[2];
} tone;

#define TONE_BASE_FREQ 16.3515978313F // C0

#define toneAddSemitone(freq, offset) (freq * pow(2, offset / 12.0))
#define toneOffset(t, semitones) (tone) {.frequency = (f32)toneAddSemitone(t.frequency, semitones), .semitoneOffset = (u16)(t.semitoneOffset + semitones)}
#define toneFromAbsoluteSemitoneOffset(offset) (tone) {.frequency = (f32)toneAddSemitone(TONE_BASE_FREQ, (offset)), .semitoneOffset = (u16)(offset)}

static_assert(sizeof(tone) == 8);

#define charIsNote(c) ((c == 'h' || c == 'H') || (c >= 'a' && c <= 'g') || (c >= 'A' && c <= 'G'))
#define charIsSharp(c) (c == '#')
#define charIsFlat(c) (c == 'b')

#define TUNING 440
#define TONE_OFFSET_C0 (12 * 0)
#define TONE_OFFSET_C1 (12 * 1)
#define TONE_OFFSET_C2 (12 * 2)
#define TONE_OFFSET_C3 (12 * 3)
#define TONE_OFFSET_C4 (12 * 4)
#define TONE_OFFSET_C5 (12 * 5)
#define TONE_OFFSET_C6 (12 * 6)
#define TONE_OFFSET_C7 (12 * 7)
#define TONE_OFFSET_C8 (12 * 8)

#define TONE_OFFSET_A4 (TONE_OFFSET_C4 - 3)
#define TONE_OFFSET_B4 (TONE_OFFSET_C5 - 1)
#define TONE_OFFSET_E4 (TONE_OFFSET_C4 + 4)
#define TONE_OFFSET_F4 (TONE_OFFSET_E4 + 1)
#define TONE_OFFSET_G4 (TONE_OFFSET_F4 + 2)

#define TONE_OFFSET_A5 (TONE_OFFSET_C5 - 3)
#define TONE_OFFSET_E5 (TONE_OFFSET_C5 + 4)
#define TONE_OFFSET_F5 (TONE_OFFSET_E5 + 1)
#define TONE_OFFSET_G5 (TONE_OFFSET_F5 + 2)

#define TONE_OFFSET_B6 (TONE_OFFSET_C7 - 1)
#define TONE_OFFSET_D6 (TONE_OFFSET_C6 + 2)
#define TONE_OFFSET_E6 (TONE_OFFSET_C6 + 4)
#define TONE_OFFSET_F6 (TONE_OFFSET_E6 + 1)
#define TONE_OFFSET_G6 (TONE_OFFSET_F6 + 2)
#define TONE_OFFSET_A6 (TONE_OFFSET_G6 + 2)

extern u8   gNoteNameToOffsetTable[];
extern char gOffsetToNoteNameTable[];

void       solfegeInit();
synthErrno solfegeParseNote(str pString, tone* pOutTone);
synthErrno solfegeSemitoneToStr(str pString, u8 pSemitones, bool pPreferFlats);
synthErrno solfegeToneWithVelocityToStr(str pString, u8 pSemitones, u8 pVelocity, bool pPreferFlats);
bool         solfegeToneIsNatural(u8 pSemitoneOffset);

#endif //SOLFEGE_H
