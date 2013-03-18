//////////////////////////////////////////////////////////////////////////
// mmRoutines.h: Multimedia routines
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"

void printAudioInfo(void);
void printMMIOError(DWORD code);
double record(int iDevice, char *fileName);
void saveWaveFile(char *fileName, WAVEFORMATEX waveFormat, WAVEHDR waveHeader);

extern const int N_SECONDS;
extern const DWORD formats[]; 
extern const int nFormats;
extern const char *formatNames[];

