#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H

#include <stdbool.h>
#include <stdio.h>

/* Saved at app->player.download */
typedef struct {
    FILE *handle;
    char downloadingFilename[1024 * 2];
    char lovedFilename[1024 * 2];
    char unlovedFilename[1024 * 2];
    unsigned int loveSong;
    bool cleanup;
} BarDownload_t;

struct audioPlayer;
struct BarApp_t;
#include <waitress.h>

/* called everytime a data block from the current song needs to be written out
 * */
void BarDownloadWrite(struct audioPlayer *player, const void *data, size_t size);

/* called at "songstart" prior to player thread being spawned */
void BarDownloadStart(struct BarApp_t *app);

/* call when the song is done or some error causing the song to end has occured
 * */
void BarDownloadFinish(struct audioPlayer *player, WaitressReturn_t wRet);

#endif /* _DOWNLOAD_H */
