#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#define IOP_CT 1024

struct io_op {
    enum type {
	IO_TYPE_WRITE,
	IO_TYPE_EXIT,
    } type;
    union {
	struct io_write {
	    void *data;
	    size_t len;
	} write;
    } data;
};

/* With a bare 'fwrite()', IO load on the system can cause audio stalls. Queue
 * blocks to be written instead and write them out in a seperate thread */
struct io_queue {
    size_t head, tail, high;
    struct io_op iops[IOP_CT];
    pthread_cond_t cond;
    /* the mutex is theoretically not required */
    pthread_mutex_t mutex;
    pthread_t thread;
};

/* Saved at app->player.download
 *
 * Keep in mind that 'player' (and thus this struct) are zeroed and
 * reinitialized after every song */
typedef struct {
    FILE *handle;
    char downloadingFilename[1024 * 2];
    char lovedFilename[1024 * 2];
    char unlovedFilename[1024 * 2];
    unsigned int loveSong;
    bool cleanup;

    struct io_queue io_ctx;
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

/* we're dieing or otherwise need to exit quickly. But this would be really
 * nice if it could happen first. */
void BarDownloadCleanup(struct BarApp_t *app);

/* on startup and shutdown of program */
void BarDownloadInit(struct BarApp_t *app);
void BarDownloadDini(struct BarApp_t *app);

#endif /* _DOWNLOAD_H */
