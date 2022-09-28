#ifndef MAKESTM_GIF_H
#define MAKESTM_GIF_H

#include <gif_lib.h>

typedef struct {
	GifFileType *fp;
	int refresh;
	int bgDispose;
} GifFile;

int OpenGifFile(GifFile *gif, const char *filename);
int GetGifFrame(GifFile *gif, char *imageConv, int index, int colorMode);
void CloseGifFile(GifFile *gif);

#endif /* MAKESTM_GIF_H */