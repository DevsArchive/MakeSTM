#include <string.h>
#include "wave.h"

int OpenWaveFile(WaveFile *wave, const char* filename)
{
	char str[5];
	int32_t chunkSize;
	int16_t fmt;

	if (wave) {
		if ((wave->fp = fopen(filename, "rb")) != NULL) {
			fread(str, 1, 4, wave->fp);
			str[4] = 0;
			if (strcmp(str, "RIFF")) {
				fclose(wave->fp);
				return 0;
			}
			fseek(wave->fp, 4, SEEK_CUR);

			fread(str, 1, 4, wave->fp);
			if (strcmp(str, "WAVE")) {
				fclose(wave->fp);
				return 0;
			}

			fread(str, 1, 4, wave->fp);
			if (strcmp(str, "fmt ")) {
				fclose(wave->fp);
				return 0;
			}

			fread(&chunkSize, 4, 1, wave->fp);
			fread(&fmt, 2, 1, wave->fp);
			if (fmt != 1) {
				fclose(wave->fp);
				return 0;
			}

			fread(&wave->channels, 2, 1, wave->fp);
			if (wave->channels > 2) {
				fclose(wave->fp);
				return 0;
			}

			fread(&wave->sampleRate, 4, 1, wave->fp);
			fseek(wave->fp, 6, SEEK_CUR);

			fread(&wave->bitDepth, 2, 1, wave->fp);
			if (wave->bitDepth != 8 && wave->bitDepth != 16) {
				fclose(wave->fp);
				return 0;
			}
			fseek(wave->fp, chunkSize - 16, SEEK_CUR);

			fread(str, 1, 4, wave->fp);
			if (strcmp(str, "LIST") == 0) {
				fread(&chunkSize, 4, 1, wave->fp);
				fseek(wave->fp, chunkSize, SEEK_CUR);
				fread(str, 1, 4, wave->fp);
			}
			if (strcmp(str, "data")) {
				fclose(wave->fp);
				return 0;
			}
			fseek(wave->fp, 4, SEEK_CUR);

			return 1;
		}
	}

	return 0;
}

int ReadWaveData(WaveFile *wave, char *buffer, int length)
{
	float audioAdv, inI;
	uint8_t audioRead[0x1000];
	int read, outI, div, pos, left;
	float samplesRead;
	uint32_t sample;

	memset(buffer, 0x80, length);

	outI = 0;
	audioAdv = wave->sampleRate / 32768.f;
	div = wave->channels * (wave->bitDepth / 8);

	if (wave && buffer && length > 0) {
		while (outI < length) {
			read = fread(audioRead, 1, 0x1000, wave->fp);
			if (read <= 0) {
				return 0;
			}
			samplesRead = (float)(read / div);

			for (inI = 0; inI < samplesRead && outI < length; inI += audioAdv) {
				switch (wave->bitDepth) {
				case 8:
					if (wave->channels == 2) {
						sample = (audioRead[(int)inI * 2] + audioRead[((int)inI * 2) + 1]) / 2;
					} else {
						sample = audioRead[(int)inI];
					}
					break;

				case 16:
					if (wave->channels == 2) {
						sample = ((((int16_t*)audioRead)[(int)inI * 2] + ((int16_t*)audioRead)[((int)inI * 2) + 1]) / 2);
					}
					else {
						sample = ((int16_t*)audioRead)[(int)inI];
					}
					sample = (sample + 0x8000) & 0xFFFF;
					if ((sample & 0xFF) >= 0x80) {
						sample += 0x100;
						if (sample >= 0x10000) {
							sample = 0xFFFF;
						}
					}
					sample >>= 8;
					break;
				}

				sample &= 0xFF;
				if (sample < 0x80) {
					sample = 0x7F - sample;
				}
				if (sample == 0xFF) {
					sample = 0xFE;
				}
				buffer[outI++] = sample;
			}

			while (outI < length && inI >= samplesRead) {
				inI -= samplesRead;
			}
		}

		if (inI > samplesRead) {
			fseek(wave->fp, read - ((int)inI * div), SEEK_CUR);
		} else {
			fseek(wave->fp, ((int)inI * div) - read, SEEK_CUR);
		}

		pos = ftell(wave->fp);
		fseek(wave->fp, 0, SEEK_END);
		left = ftell(wave->fp) - pos;
		fseek(wave->fp, pos, SEEK_SET);

		return left > 0;
	}

	return 0;
}

void CloseWaveFile(WaveFile *wave)
{
	if (wave && wave->fp) {
		wave->channels = 0;
		wave->sampleRate = 0;
		wave->bitDepth = 0;
		fclose(wave->fp);
		wave->fp = NULL;
	}
}
