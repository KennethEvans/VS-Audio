#include "stdafx.h"
#include "mmRoutines.h"

const int N_SECONDS = 10;

const DWORD formats[] = {
	WAVE_FORMAT_1M08,
	WAVE_FORMAT_1M16,
	WAVE_FORMAT_1S08,
	WAVE_FORMAT_1S16,
	WAVE_FORMAT_2M08,
	WAVE_FORMAT_2M16,
	WAVE_FORMAT_2S08,
	WAVE_FORMAT_2S16,
	WAVE_FORMAT_4M08,
	WAVE_FORMAT_4M16,
	WAVE_FORMAT_4S08,
	WAVE_FORMAT_4S16,
	WAVE_FORMAT_96M08,
	WAVE_FORMAT_96M16,
	WAVE_FORMAT_96S08,
	WAVE_FORMAT_96S16,
};
const int nFormats = sizeof(formats) / sizeof(DWORD);

const char *formatNames[] = {
	"11.025 kHz, mono, 8-bit",
	"11.025 kHz, mono, 16-bit",
	"11.025 kHz, stereo, 8-bit",
	"11.025 kHz, stereo, 16-bit",
	"22.05 kHz, mono, 8-bit",
	"22.05 kHz, mono, 16-bit",
	"22.05 kHz, stereo, 8-bit",
	"22.05 kHz, stereo, 16-bit",
	"44.1 kHz, mono, 8-bit",
	"44.1 kHz, mono, 16-bit",
	"44.1 kHz, stereo, 8-bit",
	"44.1 kHz, stereo, 16-bit",
	"96 kHz, mono, 8-bit",
	"96 kHz, mono, 16-bit",
	"96 kHz, stereo, 8-bit",
	"96 kHz, stereo, 16-bit",
};

void printMMIOError(DWORD code) {
	// Report an mmio error, if one occurred.
	if (code == 0) return;
	printf("MMIO Error. Error Code: %d", code);
}

// From code at http://www.bcbjournal.com/articles/vol2/9810/Low-level_wave_audio__part_3.htm
void saveWaveFile(char *fileName, WAVEFORMATEX waveFormat, WAVEHDR waveHeader) {
	// Declare the structures we'll need.
	MMCKINFO ChunkInfo;
	MMCKINFO FormatChunkInfo;
	MMCKINFO dataChunkInfo;

	// Open the file.
	HMMIO handle = mmioOpen(
		fileName, 0, MMIO_CREATE | MMIO_WRITE);
	if (!handle) {
		printf("Error [%d] creating file %s\n");
		return;
	}

	// Create RIFF chunk. First zero out ChunkInfo structure.
	memset(&ChunkInfo, 0, sizeof(MMCKINFO));
	ChunkInfo.fccType = mmioStringToFOURCC("WAVE", 0);
	DWORD res = mmioCreateChunk(
		handle, &ChunkInfo, MMIO_CREATERIFF);
	if(res != MMSYSERR_NOERROR) {
		printMMIOError(res);
		return;
	}

	// Create the format chunk.
	FormatChunkInfo.ckid = mmioStringToFOURCC("fmt ", 0);
	FormatChunkInfo.cksize = sizeof(WAVEFORMATEX);
	res = mmioCreateChunk(handle, &FormatChunkInfo, 0);
	if(res != MMSYSERR_NOERROR) {
		printMMIOError(res);
		return;
	}

	// Write the wave format data.
	mmioWrite(handle, (char*)&waveFormat, sizeof(waveFormat));

	// Create the data chunk.
	res = mmioAscend(handle, &FormatChunkInfo, 0);
	if(res != MMSYSERR_NOERROR) {
		printMMIOError(res);
		return;
	}
	dataChunkInfo.ckid = mmioStringToFOURCC("data", 0);
	DWORD dataSize = waveHeader.dwBytesRecorded;
	dataChunkInfo.cksize = dataSize;
	res = mmioCreateChunk(handle, &dataChunkInfo, 0);
	if(res != MMSYSERR_NOERROR) {
		printMMIOError(res);
		return;
	}

	// Write the data.
	mmioWrite(handle, (char*)waveHeader.lpData, dataSize);
	// Ascend out of the data chunk.
	mmioAscend(handle, &dataChunkInfo, 0);

	// Ascend out of the RIFF chunk (the main chunk). Failure to do 
	// this will result in a file that is unreadable by Windows95
	// Sound Recorder.
	mmioAscend(handle, &ChunkInfo, 0);
	mmioClose(handle, 0);
}

// Based on code at http://www.techmind.org/wave/
double record(int iDevice, char *fileName)
{
	const int NUMPTS = 44100 * N_SECONDS;   // N_SECONDS seconds
	int sampleRate = 44100;
	short int waveIn[NUMPTS];   // 'short int' is a 16-bit type; I request 16-bit samples below
	// for 8-bit capture, you'd use 'unsigned char' or 'BYTE' 8-bit types

	HWAVEIN hWaveIn;
	WAVEHDR waveInHdr;
	MMRESULT result;

	// Specify recording parameters
	WAVEFORMATEX waveFormat;
	waveFormat.wFormatTag=WAVE_FORMAT_PCM;     // simple, uncompressed format
	waveFormat.nChannels=1;                    //  1=mono, 2=stereo
	waveFormat.nSamplesPerSec=sampleRate;      // 44100
	waveFormat.nAvgBytesPerSec=sampleRate*2;   // = nSamplesPerSec * n.Channels * wBitsPerSample/8
	waveFormat.nBlockAlign=2;                  // = n.Channels * wBitsPerSample/8
	waveFormat.wBitsPerSample=16;              //  16 for high quality, 8 for telephone-grade
	waveFormat.cbSize=0;

	result = waveInOpen(&hWaveIn, iDevice	, &waveFormat,
		0L, 0L, WAVE_FORMAT_DIRECT);
	if (result) {
		char fault[256];
		waveInGetErrorText(result, fault, 256);
		printf("Failed to open waveform input device %d", iDevice);
		return DBL_MAX;
	}

	// Zero the input
	for(int i=0; i < NUMPTS; i++) {
		waveIn[i] = 0;
	}

	// Set up and prepare header for input
	waveInHdr.lpData = (LPSTR)waveIn;
	waveInHdr.dwBufferLength = NUMPTS*2;
	waveInHdr.dwBytesRecorded=0;
	waveInHdr.dwUser = 0L;
	waveInHdr.dwFlags = 0L;
	waveInHdr.dwLoops = 0L;
	waveInPrepareHeader(hWaveIn, &waveInHdr, sizeof(WAVEHDR));

	// Insert a wave input buffer
	result = waveInAddBuffer(hWaveIn, &waveInHdr, sizeof(WAVEHDR));
	if (result) {
		printf("Failed to read block from device %d", iDevice);
		return DBL_MAX;
	}

	// Commence sampling input
	result = waveInStart(hWaveIn);
	if (result) {
		printf("Failed to start recording for device %d", iDevice);
		return DBL_MAX;
	}


	// Wait until finished recording
	do {
		// Do nothing
	} while(waveInUnprepareHeader(hWaveIn, &waveInHdr,
		sizeof(WAVEHDR))==WAVERR_STILLPLAYING);

	waveInClose(hWaveIn);

	// Write the file
	if(fileName) {
		saveWaveFile(fileName, waveFormat, waveInHdr);
	}

	// Return the average
	double sum = 0.0;
	for(int i=0; i < NUMPTS; i++) {
		sum += waveIn[i]>0?waveIn[i]:-waveIn[i];
	}
	sum /= NUMPTS;

	return sum;
}

void printAudioInfo(void) {
	printf("MM Audio Info\n");
	UINT nDevices = waveInGetNumDevs();
	printf("Number of devices: %d\n", nDevices);
	MMRESULT res = MMSYSERR_NOERROR;
	WAVEINCAPS wic;
	int nFormatsSupported;
	double canRecord = 0;
	for(UINT i=0; i < nDevices; i++) {
		res = waveInGetDevCaps(i, &wic,  sizeof(WAVEINCAPS));
		if(res != MMSYSERR_NOERROR) {
			printf("Error getting information for audio device &d", i);
			continue;
		} else {
			printf("%d %s\n", i, wic.szPname);
			printf("  Channels: %d\n", wic.wChannels);
			nFormatsSupported = 0;
			for(int j=0; j < nFormats; j++) {
				if(wic.dwFormats | formats[j] ) {
					nFormatsSupported++;
					//printf("  %s\n", formatNames[j]);
				}
			}
			printf("  Supports %d of %d standard formats\n",
				nFormatsSupported, nFormats);
			printf("  Trying to record for %d sec...\n", N_SECONDS);
			char fileName[1024];
			sprintf_s(fileName, "MM-AudioTest-%s.wav", wic.szPname);
			canRecord = record(i, fileName);
			if(canRecord == DBL_MAX) {
				printf("  Cannot record\n");
			} else {
				printf("  Can record, Average absolute level=%.2f\n",
					canRecord);
				printf("    Output is %s\n", fileName);
			}
		}
	}
}
