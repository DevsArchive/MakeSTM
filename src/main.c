#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gif.h"
#include "wave.h"

static char audioOut[0x8000];
static char imageOut[0x3820];

static int WritePacketFrames(GifFile *gif, FILE *outFile, int colorMode, int gifImage, int i) {
	while (i < 8) {
		if (gifImage < gif->fp->ImageCount) {
			if (GetGifFrame(gif, imageOut, gifImage, colorMode)) {
				fwrite(imageOut, 1, 0x3820, outFile);
			} else {
				printf("Frame %i has too many colors. The GIF must have at maximum 16 colors.\n", gifImage);
				return -1;
			}
			++gifImage;
		} else {
			fwrite(imageOut, 1, 0x3820, outFile);
		}
		++i;
	}

	memset(imageOut, 0, 0x1700);
	fwrite(imageOut, 1, 0x1700, outFile);
	return gifImage;
}

int main(int argc, char* argv[])
{
	GifFile gif;
	WaveFile wave;
	FILE *outFile;
	int i, delay, colorMode, syncMode, audioRead, gifImage;

	if (argc < 6) {
		printf("USAGE: MakeSTM [GIF file] [Wave file] [Color mode] [Sync mode] [Output file]\n\n"
			   "Note: Only supports the opening FMV format so far!\n\n"
		       "Color mode:\n"
			   "    0 = Hardware\n"
			   "    1 = Multiples of 16\n"
			   "    2 = Multiples of 18\n\n"
		       "Sync mode:\n"
		       "    0 = Don't auto-sync video and audio\n"
		       "    1 = Enable auto-sync\n");
		exit(1);
	}

	colorMode = atoi(argv[3]);
	if (colorMode < 0 || colorMode > 2) {
		printf("Invalid color mode \"%i\".\n", colorMode);
		exit(1);
	}

	syncMode = atoi(argv[4]);
	if (syncMode < 0) {
		printf("Invalid pad mode \"%i\".\n", colorMode);
		exit(1);
	}

	if (!OpenGifFile(&gif, argv[1])) {
		printf("Failed to open \"%s\" as a GIF file.\n", argv[1]);
		exit(1);
	}

	if (!OpenWaveFile(&wave, argv[2])) {
		CloseGifFile(&gif);
		printf("Failed to open \"%s\" as a wave file.\n", argv[2]);
		exit(1);
	}

	if ((outFile = fopen(argv[5], "wb")) == NULL) {
		CloseGifFile(&gif);
		CloseWaveFile(&wave);
		printf("Failed to open \"%s\" for writing.\n", argv[5]);
		exit(1);
	}

	audioRead = 1;
	gifImage = 0;
	delay = 0;
	if (syncMode) {
		delay = 4;
	}

	if (syncMode) {
		/* Pad out by a packet if auto-sync is set, to allow the audio to start properly */
		memset(audioOut, 0x80, 0x8000);
		fwrite(audioOut, 1, 0x8000, outFile);
		memset(imageOut, 0, 0x3820);
		for (i = 0; i < 8; ++i) {
			fwrite(imageOut, 1, 0x3820, outFile);
		}
		fwrite(imageOut, 1, 0x1700, outFile);
	}

	while (audioRead) {
		memset(audioOut, 0x80, 0x8000);
		audioRead = ReadWaveData(&wave, audioOut, 0x8000);
		fwrite(audioOut, 1, 0x8000, outFile);

		if (audioRead) {
			i = 0;
			memset(imageOut, 0, 0x3820);
			while (delay > 0 && i < 8) {
				/* Delay video if auto-sync is set, to get the video in sync with the audio */
				fwrite(imageOut, 1, 0x3820, outFile);
				++i;
				--delay;
			}

			gifImage = WritePacketFrames(&gif, outFile, colorMode, gifImage, i);
			if (gifImage < 0) {
				CloseGifFile(&gif);
				CloseWaveFile(&wave);
				exit(1);
			}
		}
		else {
			if (syncMode) {
				/* Pad out by 2 packets if auto-sync is set, to allow the video to finish properly */
				for (i = 0; i < 2; ++i) {
					gifImage = WritePacketFrames(&gif, outFile, colorMode, gifImage, 0);
					if (gifImage < 0) {
						CloseGifFile(&gif);
						CloseWaveFile(&wave);
						exit(1);
					}
					memset(audioOut, 0x80, 0x8000);
					fwrite(audioOut, 1, 0x8000, outFile);
				}
			}

			memset(imageOut, 0, 0x1000);
			fwrite(imageOut, 1, 0x1000, outFile);
		}
	}

	CloseGifFile(&gif);
	CloseWaveFile(&wave);

	return 0;
}
