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
static GifColorType canvasPal[256];

static int inline GetMDColor(int color, int mode)
{
	int i;
	int *colorTable;
	
	colorTable = colorTables[mode];
	for (i = 0; i < 7; ++i) {
		if (color >= colorTable[i] && color <= colorTable[i + 1]) {
			if ((colorTable[i + 1] - color) < (color - colorTable[i])) {
				return (i + 1) * 2;
			}
			return i * 2;
		}
	}
	return 0;
}

static int CheckValidImage()
{
	char flags[256];
	int i, mode, colorCount;

	mode = 0;
	colorCount = 0;
	memset(flags, 0, 256);

	for (i = 0; i < CANVAS_WIDTH * CANVAS_HEIGHT; ++i) {
		if (canvasImage[i] > 15) {
			mode = 1;
		}
		if (flags[canvasImage[i]] == 0) {
			flags[canvasImage[i]] = 1;
			++colorCount;
		}
	}
	if (colorCount <= 16) {
		++mode;
	}
	return mode;
}

static void inline GetColor(GifColorType *color, int index, int colorMode)
{
	if (color) {
		color->Red = GetMDColor(canvasPal[index].Red, colorMode);
		color->Green = GetMDColor(canvasPal[index].Green, colorMode);
		color->Blue = GetMDColor(canvasPal[index].Blue, colorMode);
	}
}

static void inline StoreColor(char *imageConv, int r, int g, int b, int index)
{
	if (imageConv) {
		imageConv[index * 2] = b;
		imageConv[(index * 2) + 1] = (g << 4) | r;
	}
}

static void ConvertImage(GifFile *gif, SavedImage *image, char *imageConv, int mode, int colorMode)
{
	GifColorType color;
	int x, y, x2, y2, i, j, trns;
	int palConv[256];
	int palIndex, px1, px2, w, h;

	if (gif && gif->fp && image && imageConv && mode > 0) {
		memset(palConv, -1, 256 * sizeof(int));
		palIndex = 0;

		/* Get pixels */
		for (y = 0; y < image->ImageDesc.Height; ++y) {
			for (x = 0; x < image->ImageDesc.Width; ++x) {
				i = x + (y * image->ImageDesc.Width);
				trns = 0;
				for (j = 0; j < image->ExtensionBlockCount; ++j) {
					if (image->ExtensionBlocks[j].Function == 0xF9) {
						if (image->RasterBits[i] == image->ExtensionBlocks[j].Bytes[3]) {
							trns = 1;
							break;
						}
					}
				}
				if (!trns && (x + image->ImageDesc.Left) < CANVAS_WIDTH &&
				    (y + image->ImageDesc.Top) < CANVAS_HEIGHT) {
					canvasImage[x + image->ImageDesc.Left + ((y + image->ImageDesc.Top) * CANVAS_WIDTH)] = image->RasterBits[i];
				}
			}
		}

		/* Get palette */
		if (image->ImageDesc.ColorMap) {
			for (i = 0; i < image->ImageDesc.ColorMap->ColorCount; ++i) {
				canvasPal[i] = image->ImageDesc.ColorMap->Colors[i];
			}
		} else {
			for (i = 0; i < gif->fp->SColorMap->ColorCount; ++i) {
				canvasPal[i] = gif->fp->SColorMap->Colors[i];
			}
		}

		/* Save pixels */
		i = 0x20;
		for (y = 0; y < CANVAS_HEIGHT; y += 8) {
			for (x = 0; x < CANVAS_WIDTH; x += 8) {
				for (y2 = 0; y2 < 8; ++y2) {
					for (x2 = 0; x2 < 8; x2 += 2) {
						j = (x + x2) + ((y + y2) * CANVAS_WIDTH);
						px1 = canvasImage[j];
						px2 = canvasImage[j + 1];
						if (mode == 2) {
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
		if (mode == 2) {
			for (i = 0; i < 256; ++i) {
				color.Red = 0;
				color.Green = 0;
				color.Blue = 0;
				if (palConv[i] >= 0) {
					GetColor(&color, i, colorMode);
					StoreColor(imageConv, color.Red, color.Green, color.Blue, palConv[i]);
				}
			}
		} else {
			for (i = 0; i < 16; ++i) {
				color.Red = 0;
				color.Green = 0;
				color.Blue = 0;
				GetColor(&color, i, colorMode);
				StoreColor(imageConv, color.Red, color.Green, color.Blue, i);
			}
		}
	}
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
		return 1;
	}
	return 0;
}

int GetGifFrame(GifFile *gif, char *imageConv, int index, int colorMode)
{
	int mode;

	if (gif && gif->fp) {
		mode = 0;
		if (gif->fp->SavedImages[index].ImageDesc.ColorMap) {
			if (gif->fp->SavedImages[index].ImageDesc.ColorMap->ColorCount <= 16) {
				mode = 1;
			}
		} else {
			if (gif->fp->SColorMap->ColorCount <= 16) {
				mode = 1;
			}
		} if (mode == 0) {
			mode = CheckValidImage(gif->fp->SavedImages + index);
		}

		if (mode > 0) {
			ConvertImage(gif, gif->fp->SavedImages + index, imageConv, mode, colorMode);
			return 1;
		}
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
