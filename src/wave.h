#ifndef MAKESTM_WAVE_H
#define MAKESTM_WAVE_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
	FILE *fp;
	int16_t channels;
	int32_t sampleRate;
	int16_t bitDepth;
} WaveFile;

int OpenWaveFile(WaveFile *wave, const char *filename);
int ReadWaveData(WaveFile *wave, char *buffer, int length);
void CloseWaveFile(WaveFile *wave);

#endif /* MAKESTM_WAVE_H */