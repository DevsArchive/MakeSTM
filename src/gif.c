#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gif.h"

#define CANVAS_WIDTH 256
#define CANVAS_HEIGHT 112

static int hardwareColors[] = {
	0, 52, 87, 116, 144, 172, 206, 255
};

static int mul16Colors[] = {
	0, 32, 64, 96, 128, 160, 192, 224
};

static int mul18Colors[] = {
	0, 36, 73, 109, 145, 182, 218, 255
};

static int *colorTables[] = {
	hardwareColors,
	mul16Colors,
	mul18Colors
};

static char canvasImage[CANVAS_WIDTH * CANVAS_HEIGHT];
static char savedCanvas[CANVAS_WIDTH * CANVAS_HEIGHT];
static GifColorType canvasPal[256];
static GifColorType savedPal[256];

static int inline GetMDColor(int color, int mode)
{
	int i;
	int *colorTable;
	
	colorTable = colorTables[mode];
	for (i = 0; i < 8 - 1; ++i) {
		if (color >= colorTable[i] && color <= colorTable[i + 1]) {
			if ((colorTable[i + 1] - color) < (color - colorTable[i])) {
				return (i + 1) * 2;
			}
			return i * 2;
		}
	}
	return 0;
}

static int ConvertImage(GifFile *gif, SavedImage *image, char *imageConv, int colorMode)
{
	GraphicsControlBlock gcb;
	ColorMapObject *colorMap;
	GifColorType palMDConv[256];
	GifColorType palCombine[256];
	GifColorType palNew[256];
	int palConv[256];
	char palFlags[256];
	int mode, trns, dispose, palIndex, colorCount, black;
	int x, y, x2, y2, i, i2, j, px1, px2;

	if (gif && gif->fp && image && imageConv) {
		/* Get palette and convert it to be MD compatible */
		colorMap = gif->fp->SColorMap;
		if (image->ImageDesc.ColorMap) {
			colorMap = image->ImageDesc.ColorMap;
		}
		memset(palMDConv, 0, 256 * sizeof(GifColorType));
		for (i = 0; i < colorMap->ColorCount; ++i) {
			palMDConv[i].Red = GetMDColor(colorMap->Colors[i].Red, colorMode);
			palMDConv[i].Green = GetMDColor(colorMap->Colors[i].Green, colorMode);
			palMDConv[i].Blue = GetMDColor(colorMap->Colors[i].Blue, colorMode);
		}

		/* Get disposal method and transparent color */
		trns = NO_TRANSPARENT_COLOR;
		dispose = DISPOSAL_UNSPECIFIED;
		for (i = 0; i < image->ExtensionBlockCount; ++i) {
			if (image->ExtensionBlocks[i].Function == 0xF9) {
				DGifExtensionToGCB(image->ExtensionBlocks[i].ByteCount, image->ExtensionBlocks[i].Bytes, &gcb);
				dispose = gcb.DisposalMode;
				trns = gcb.TransparentColor;
			}
		}

		/* Get palette */
		if (!gif->refresh) {
			palIndex = 0;
			black = 0;
			memset(palCombine, 0, 256 * sizeof(GifColorType));
			memset(palNew, 0, 256 * sizeof(GifColorType));

			for (i = 0; i < 256; ++i) {
				for (j = 0; j < 256; ++j) {
					if (canvasPal[i].Red == palCombine[j].Red &&
						canvasPal[i].Green == palCombine[j].Green &&
						canvasPal[i].Blue == palCombine[j].Blue) {
						break;
					}
				}
				if (j == 256 || (!black && (canvasPal[i].Red | canvasPal[i].Green | canvasPal[i].Blue) == 0)) {
					if ((canvasPal[i].Red | canvasPal[i].Green | canvasPal[i].Blue) == 0) {
						black = 1;
					}
					palCombine[palIndex++] = canvasPal[i];
				}
			}

			for (i = 0; i < 256; ++i) {
				for (j = 0; j < 256; ++j) {
					if (palMDConv[i].Red == palCombine[j].Red &&
						palMDConv[i].Green == palCombine[j].Green &&
						palMDConv[i].Blue == palCombine[j].Blue) {
						break;
					}
				}
				if (j == 256 || (!black && (canvasPal[i].Red | canvasPal[i].Green | canvasPal[i].Blue) == 0)) {
					if ((canvasPal[i].Red | canvasPal[i].Green | canvasPal[i].Blue) == 0) {
						black = 1;
					}
					palCombine[palIndex++] = palMDConv[i];
				}
			}
		} else {
			memcpy(palNew, palMDConv, 256 * sizeof(GifColorType));
		}

		/* Compile frame */
		if (!gif->refresh) {
			for (y = 0; y < CANVAS_HEIGHT; ++y) {
				for (x = 0; x < CANVAS_WIDTH; ++x) {
					i = x + (y * CANVAS_WIDTH);
					for (j = 0; j < 256; ++j) {
						if (canvasPal[canvasImage[i]].Red == palCombine[j].Red &&
							canvasPal[canvasImage[i]].Green == palCombine[j].Green &&
							canvasPal[canvasImage[i]].Blue == palCombine[j].Blue) {
							canvasImage[i] = j;
							break;
						}
					}
				}
			}
			for (y = 0; y < image->ImageDesc.Height; ++y) {
				for (x = 0; x < image->ImageDesc.Width; ++x) {
					if ((x + image->ImageDesc.Left) < CANVAS_WIDTH && (y + image->ImageDesc.Top) < CANVAS_HEIGHT) {
						px1 = image->RasterBits[x + (y * image->ImageDesc.Width)];
						if (px1 != trns) {
							for (j = 0; j < 256; ++j) {
								if (palMDConv[px1].Red == palCombine[j].Red &&
									palMDConv[px1].Green == palCombine[j].Green &&
									palMDConv[px1].Blue == palCombine[j].Blue) {
									canvasImage[x + image->ImageDesc.Left + ((y + image->ImageDesc.Top) * CANVAS_WIDTH)] = j;
									break;
								}
							}
						}
					}
				}
			}
		} else {
			for (y = 0; y < image->ImageDesc.Height; ++y) {
				for (x = 0; x < image->ImageDesc.Width; ++x) {
					if ((x + image->ImageDesc.Left) < CANVAS_WIDTH && (y + image->ImageDesc.Top) < CANVAS_HEIGHT) {
						px1 = image->RasterBits[x + (y * image->ImageDesc.Width)];
						if (px1 != trns) {
							canvasImage[x + image->ImageDesc.Left + ((y + image->ImageDesc.Top) * CANVAS_WIDTH)] = px1;
						}
					}
				}
			}
		}

		/* Check color count */
		mode = 0;
		colorCount = 0;
		palIndex = 0;
		memset(palFlags, 0, 256);
		memset(palConv, -1, 256 * sizeof(int));
		for (y = 0; y < CANVAS_HEIGHT; ++y) {
			for (x = 0; x < CANVAS_WIDTH; ++x) {
				i = x + (y * CANVAS_WIDTH);
				if (!gif->refresh) {
					/* Only store used colors */
					if (palConv[canvasImage[i]] < 0) {
						palNew[colorCount] = palCombine[canvasImage[i]];
						palConv[canvasImage[i]] = colorCount;
					}
					canvasImage[i] = palConv[canvasImage[i]];
				}
				if (canvasImage[i] > 0xF) {
					mode = 1;
				}
				if (!palFlags[canvasImage[i]]) {
					palFlags[canvasImage[i]] = 1;
					++colorCount;
				}
			}
		}
		if (colorCount > 16) {
			return 0;
		}
		memcpy(canvasPal, palNew, 256 * sizeof(GifColorType));

		/* Save pixels */
		i = 0x20;
		palIndex = 0;
		memset(palConv, -1, 256 * sizeof(int));
		for (y = 0; y < CANVAS_HEIGHT; y += 8) {
			for (x = 0; x < CANVAS_WIDTH; x += 8) {
				for (y2 = 0; y2 < 8; ++y2) {
					for (x2 = 0; x2 < 8; x2 += 2) {
						j = (x + x2) + ((y + y2) * CANVAS_WIDTH);
						px1 = canvasImage[j];
						px2 = canvasImage[j + 1];
						if (mode == 1) {
							if (palConv[px1] < 0) {
								palConv[px1] = palIndex++;
							}
							if (palConv[px2] < 0) {
								palConv[px2] = palIndex++;
							}
							imageConv[i++] = (palConv[px1] << 4) | palConv[px2];
						} else {
							imageConv[i++] = (px1 << 4) | px2;
						}
					}
				}
			}
		}

		/* Save palette */
		if (mode == 1) {
			for (i = 0; i < 256; ++i) {
				if (palConv[i] >= 0) {
					imageConv[palConv[i] * 2] = canvasPal[i].Blue;
					imageConv[(palConv[i] * 2) + 1] = (canvasPal[i].Green << 4) | canvasPal[i].Red;
				}
			}
		} else {
			for (i = 0; i < 16; ++i) {
				imageConv[i * 2] = canvasPal[i].Blue;
				imageConv[(i * 2) + 1] = (canvasPal[i].Green << 4) | canvasPal[i].Red;
			}
		}

		/* Dispose */
		switch (dispose) {
		case DISPOSAL_UNSPECIFIED:
		case DISPOSE_DO_NOT:
			/* Don't dispose */
			memcpy(savedCanvas, canvasImage, CANVAS_WIDTH * CANVAS_HEIGHT);
			memcpy(savedPal, canvasPal, 256 * sizeof(GifColorType));
			gif->refresh = 0;
			break;

		case DISPOSE_BACKGROUND:
			/* Dispose to background */
			memset(canvasImage, gif->fp->SBackGroundColor, CANVAS_WIDTH * CANVAS_HEIGHT);
			gif->refresh = 1;
			break;

		case DISPOSE_PREVIOUS:
			/* Dispose to previous */
			memcpy(canvasImage, savedCanvas, CANVAS_WIDTH * CANVAS_HEIGHT);
			memcpy(canvasPal, savedPal, 256 * sizeof(GifColorType));
			gif->refresh = 1;
			break;
		}

		return 1;
	}

	return 0;
}

int OpenGifFile(GifFile *gif, const char *filename)
{
	int errorCode;

	if (gif) {
		if ((gif->fp = DGifOpenFileName(filename, &errorCode)) == NULL) {
			return 0;
		} else if (DGifSlurp(gif->fp) != GIF_OK) {
			DGifCloseFile(gif->fp, &errorCode);
			return 0;
		}
		memset(canvasImage, gif->fp->SBackGroundColor, CANVAS_WIDTH * CANVAS_HEIGHT);
		gif->refresh = 1;
		return 1;
	}
	return 0;
}

int GetGifFrame(GifFile *gif, char *imageConv, int index, int colorMode)
{
	if (gif && gif->fp) {
		return ConvertImage(gif, gif->fp->SavedImages + index, imageConv, colorMode);
	}
	return 0;
}

void CloseGifFile(GifFile *gif)
{
	int errorCode;

	if (gif && gif->fp) {
		DGifCloseFile(gif->fp, &errorCode);
		gif->fp = NULL;
	}
}
