#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H

#include <piano.h>
#include "player.h"
#include "main.h"

void BarDownloadStart(BarApp_t *);
void BarDownloadWrite(struct audioPlayer *, char *, size_t);
void BarDownloadFinish(struct audioPlayer *, WaitressReturn_t);

#endif /* _DOWNLOAD_H */
