#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H

#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>

#define IOP_CT 1024

struct io_op {
    enum type {
	IO_TYPE_OPEN,
	IO_TYPE_WRITE,
	IO_TYPE_RENAME,
	IO_TYPE_CLOSE,
	IO_TYPE_UNLINK,
	IO_TYPE_MKDIR,
	IO_TYPE_EXIT,
    } type;
    union {
#define OPEN_FMT "openat(%d, %s, %d, %d)"
#define OPEN_EXP(iop)		\
	iop.data.open.dirfd,	\
	iop.data.open.pathname,	\
	iop.data.open.flags,	\
	iop.data.open.mode
	struct io_open {
	    char *pathname;
	    int dirfd;
	    int flags;
	    mode_t mode;
	} open;
	struct io_write {
	    void *data;
	    size_t len;
	    int fd;
	} write;
#define RENAME_FMT "renameat(%d, %s, %d, %s)"
#define RENAME_EXP(iop)			\
	iop.data.rename.oldfd,		\
	iop.data.rename.old,		\
	iop.data.rename.newfd,		\
	iop.data.rename.new
	struct io_rename {
	    int oldfd;
	    int newfd;
	    char *old;
	    char *new;
	} rename;
	struct io_close {
	    int fd;
	} close;
#define UNLINK_FMT "unlink(%d, %s, %d)"
#define UNLINK_EXP(iop)			\
	iop.data.unlink.dirfd,		\
	iop.data.unlink.pathname,	\
	iop.data.unlink.flags
	struct ioq_unlink {
	    char *pathname;
	    int dirfd;
	    int flags;
	} unlink;
#define MKDIR_FMT "mkdirat(%d, %s, %d)"
#define MKDIR_EXP(iop)		\
	iop.data.mkdir.fd,	\
	iop.data.mkdir.path,	\
	iop.data.mkdir.mode
	struct ioq_mkdir {
	    char *path;
	    int fd;
	    mode_t mode;
	} mkdir;
	/* io_exit has no args */
    } data;
};

/* With a bare 'fwrite()', IO load on the system can cause audio stalls. Queue
 * blocks to be written instead and write them out in a seperate thread */
struct io_queue {
    bool overflow;
    size_t head, tail, high, processed;
    struct io_op iops[IOP_CT];
    /* We have 1 consumer and 1 producer, the mutex is theoretically not
     * required if we use atomics to update the head & tail */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;
    FILE *single_file;
};

/* Saved at app->player.download
 *
 * Keep in mind that 'player' (and thus this struct) are zeroed and
 * reinitialized after every song */
typedef struct {
    char downloadingFilename[1024 * 2];
    char lovedFilename[1024 * 2];
    char unlovedFilename[1024 * 2];

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
