#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 201401L /* fileno() */
#define _ATFILE_SOURCE
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <string.h> /* strdup() */
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h> /* mkdir() */


#include "download.h"
#include "ui.h" /* BarUiMsg */
#include "circ_buf.h"

#define CURR_FD -2

#define container_off(containing_type, member)	\
	offsetof(containing_type, member)
#define container_of(member_ptr, containing_type, member)		\
	 ((containing_type *)						\
	  (void *)((char *)(member_ptr)						\
	   - container_off(containing_type, member)))

static void *memdup(const void *data, size_t len)
{
	void *x = malloc(len);
	if (!x)
		return NULL;

	memcpy(x, data, len);
	return x;
}

static int _ioq_check_for_space(struct io_queue *ioq)
{
	size_t curr = CIRC_CNT(ioq->head, ioq->tail, IOP_CT);
	if (curr > ioq->high)
		ioq->high = curr;
	if (CIRC_FULL(ioq->head, ioq->tail, IOP_CT)) {
		fprintf(stderr, "CIRC_FULL head: %zu tail: %zu\n", ioq->head, ioq->tail);
		ioq->overflow = true;
		return -1;
	}

	return 0;
}

__attribute__((format(printf,1,2)))
static inline void printf_check(const char *fmt, ...)
{
}

#ifdef DEBUG
#define ioq_debug(fmt, ...) \
		fprintf(stderr, "ioq: " fmt, ## __VA_ARGS__)
#else
/* FIXME: eval arguments */
#define ioq_debug(...) printf_check(__VA_ARGS__)
#endif

#ifdef DEBUG
#define iop_log(fmt, ...) \
		fprintf(stderr, fmt, ## __VA_ARGS__)
#else
#define iop_log(...) printf_check(__VA_ARGS__)
#endif

static void _ioq_wait_for_space(struct io_queue *ioq)
{
	size_t curr = CIRC_CNT(ioq->head, ioq->tail, IOP_CT);
	if (curr > ioq->high)
		ioq->high = curr;
	while (CIRC_FULL(ioq->head, ioq->tail, IOP_CT)) {
		fprintf(stderr, "CIRC_FULL head: %zu tail: %zu\n", ioq->head, ioq->tail);
		ioq->overflow = true;
		pthread_cond_wait(&ioq->cond, &ioq->mutex);
	}
}

static struct io_op *ioq_add__lock(struct io_queue *ioq)
{
	pthread_mutex_lock(&ioq->mutex);
	int r = _ioq_check_for_space(ioq);
	if (r)
		return NULL;
	return &ioq->iops[ioq->head];
}

static void _ioq_op_added(struct io_queue *ioq)
{
	//if (CIRC_EMPTY(ioq->head, ioq->tail, IOP_CT))
		pthread_cond_signal(&ioq->cond);
	ioq->head = CIRC_NEXT(ioq->head, IOP_CT);
}

static void ioq_add__unlock(struct io_queue *ioq)
{
	ioq_debug("added %d\n", ioq->iops[ioq->head].type);
	_ioq_op_added(ioq);
	pthread_mutex_unlock(&ioq->mutex);
}

static void io_write(struct io_queue *ioq, int filedes, const void *data, size_t size)
{
	struct io_op *iop = ioq_add__lock(ioq);

	*iop = (struct io_op) {
		.type = IO_TYPE_WRITE,
		.data.write = {
			.fd = filedes,
			.data = memdup(data, size),
			.len  = size,
		}
	};

	ioq_add__unlock(ioq);
}

static void io_mkdir(struct io_queue *ioq, int fd, const char *path, mode_t mode)
{
	struct io_op *iop = ioq_add__lock(ioq);

	*iop = (struct io_op) {
		.type = IO_TYPE_MKDIR,
		.data.mkdir = {
			.fd = fd,
			.path = strdup(path),
			.mode = mode
		}
	};

	ioq_add__unlock(ioq);
}

static void ioq_join(struct io_queue *ioq)
{
	struct io_op *iop = ioq_add__lock(ioq);
	iop->type = IO_TYPE_EXIT;
	ioq_add__unlock(ioq);
	pthread_join(ioq->thread, NULL);
}

static void io_rename(struct io_queue *ioq, int oldfd, const char *old, int newfd, const char *new)
{
	struct io_op *op = ioq_add__lock(ioq);
	*op = (struct io_op) {
		.type = IO_TYPE_RENAME,
		.data.rename = {
			.oldfd = oldfd,
			.old = strdup(old),
			.newfd = newfd,
			.new = strdup(new)
		}
	};
	ioq_add__unlock(ioq);
}

static void io_close(struct io_queue *ioq, int fd)
{
	struct io_op *op = ioq_add__lock(ioq);
	*op = (struct io_op) {
		.type = IO_TYPE_CLOSE,
		.data.close = {
			.fd = fd
		}
	};
	ioq_add__unlock(ioq);
}

static void io_unlink(struct io_queue *ioq, int dirfd, const char *pathname, int flags)
{
	struct io_op *op = ioq_add__lock(ioq);
	*op = (struct io_op) {
		.type = IO_TYPE_UNLINK,
		.data.unlink = {
			.dirfd = dirfd,
			.pathname = strdup(pathname),
			.flags = flags
		}
	};
	ioq_add__unlock(ioq);
}

static void *io_thread(void *v) {
	struct io_queue *io = v;
	int ret;

	for (;;) {
		pthread_mutex_lock(&io->mutex);
		while (CIRC_EMPTY(io->head, io->tail, IOP_CT))
			pthread_cond_wait(&io->cond, &io->mutex);

		/* copy the iop */
		struct io_op iop = io->iops[io->tail];

		/* advance the circular buffer */
		if (CIRC_FULL(io->head, io->tail, IOP_CT))
			pthread_cond_signal(&io->cond);

		io->tail = CIRC_NEXT(io->tail, IOP_CT);
		pthread_mutex_unlock(&io->mutex);

		/* process the iop */
		switch(iop.type) {
			case IO_TYPE_OPEN:
				if (io->single_file) {
					printf("WARN: open while file still open\n");
					fclose(io->single_file);
				}

				assert(iop.data.open.dirfd == AT_FDCWD);
				assert(iop.data.open.flags == O_WRONLY);
				assert(iop.data.open.mode  == 0666);

				io->single_file = fopen(iop.data.open.pathname, "w");
				iop_log(OPEN_FMT " = %p\n",
						OPEN_EXP(iop),
						(void *)io->single_file);
				if (!io->single_file) {
					printf("WARN: failed to open %s\n", iop.data.open.pathname);
				}

				free(iop.data.open.pathname);
				break;

			case IO_TYPE_WRITE:
				assert(iop.data.write.fd == CURR_FD);

				if (!io->single_file) {
					/* looks like opening failed, path name was probably too long */
					iop_log("write() -> NOP\n");
					goto nop_write;
				}

				{
					ssize_t r = fwrite(iop.data.write.data, iop.data.write.len, 1,
								io->single_file);
					iop_log("write(..., %zu, %p) = %zd\n",
							iop.data.write.len,
							(void *)io->single_file, r);
				}
nop_write:
				free(iop.data.write.data);
				break;
			case IO_TYPE_CLOSE:
				assert(iop.data.close.fd == CURR_FD);
				if (!io->single_file) {
					iop_log("close() -> NOP\n");
					break;
				}

				ret = fclose(io->single_file);
				iop_log("close(%p) = %d\n", (void *)io->single_file, ret);
				io->single_file = NULL;
				break;
			case IO_TYPE_MKDIR:
				ret = mkdirat(MKDIR_EXP(iop));
				iop_log(MKDIR_FMT" = %d\n",
						MKDIR_EXP(iop), ret);
				if (ret && errno != EEXIST)
					printf("WARN: "MKDIR_FMT" =  %d %d %s\n", MKDIR_EXP(iop), ret, errno, strerror(errno));
				free(iop.data.mkdir.path);
				break;
			case IO_TYPE_RENAME:
				ret = renameat(RENAME_EXP(iop));
				iop_log(RENAME_FMT" = %d\n",
						RENAME_EXP(iop), ret);
				if (ret)
					printf("WARN: "RENAME_FMT" = %d\n",
							RENAME_EXP(iop),
						ret);
				free(iop.data.rename.new);
				free(iop.data.rename.old);
				break;
			case IO_TYPE_UNLINK:
				ret = unlinkat(UNLINK_EXP(iop));
				iop_log(UNLINK_FMT" = %d\n",
						UNLINK_EXP(iop), ret);
				if (ret)
					printf("WARN: "UNLINK_FMT" = %d\n",
						UNLINK_EXP(iop),
						ret);

				free(iop.data.unlink.pathname);
				break;
			case IO_TYPE_EXIT:
				return NULL;
			default:
				/* racy, but we don't care */
				printf("ERROR: invalid iotype %d\n", iop.type);
				abort();
		}
	}
}

static void io_open(struct io_queue *ioq, int dirfd, const char *pathname, int flags, mode_t mode)
{
	struct io_op *op = ioq_add__lock(ioq);

	*op = (struct io_op) {
		.type = IO_TYPE_OPEN,
		.data.open = {
			.dirfd = dirfd,
			.pathname = strdup(pathname),
			.flags = flags,
			.mode = mode
		}
	};

	ioq_add__unlock(ioq);
}


static void ioq_init(struct io_queue *ioq)
{
	pthread_mutex_init(&ioq->mutex, NULL);
	pthread_cond_init(&ioq->cond, NULL);
	pthread_create(&ioq->thread, NULL, io_thread, ioq);
}

static BarApp_t *player_to_app(struct audioPlayer *player)
{
	return container_of(player, BarApp_t, player);
}


bool _nchar( char c ){
	if ( 48 <= c && c <= 57 ) { /* 0 .. 9 */
		return true;
	}
	if ( 65 <= c && c <= 90 ) { /* A .. Z */
		return true;
	}
	else if ( 97 <= c && c <= 122 ) { /* a .. z */
		return true;
	}
	else if ( c == 95 ) { /* _ */
		return true;
	}
	return false;
}

static char *_nstrdup( const char *s0 ){
	char *s1 = malloc( strlen( s0 ) + 1 );
	char *s1i = 0;
	memset( s1, 0, strlen( s0 ) + 1 );

	s1i = s1;
	while( *s0 ){

		if ( _nchar( *s0 ) ) {
			/* Normal character, A-Za-z_, just copy it */
			*s1i = *s0;
			s1i++;
		}
		else {
			/* Not a normal character, attempt to replace with _ ... */
			if ( s1i == s1 ) {
				/* At the beginning of the string, just skip */
			} 
			else if ( *(s1i - 1) == '_' ) {
				/* Already have a _, just skip */
			}
			else {
				*s1i = '_';
				s1i++;
			}
		}

		s0++;
	}

	if ( *(s1i - 1) == '_' ) {
		/* Strip trailing _ */
		*(s1i - 1) = 0;
	}

	return s1;
}

static char *_slash2dash_strdup( const char *s0 ){
	char *s1 = strdup( s0 );
	char *s1i = s1;
	while ( s1i = strchr( s1i, '/' ) ) {
		*s1i = '-';
	}
	return s1;
}

static void BarDownloadFilename(BarApp_t *app) {
	char baseFilename[1024 * 2];
	char songFilename[1024 * 2];
	const char *separator = 0;
	PianoSong_t *song = app->playlist;
	PianoStation_t *station = app->curStation;
	BarDownload_t *download = &app->download;
	struct io_queue *ioq = &download->io_ctx;

	memset(songFilename, 0, sizeof (songFilename));
	memset(baseFilename, 0, sizeof (baseFilename));

	separator = app->settings.downloadSeparator;

	{
		char *artist = 0, *album = 0, *title = 0;

		if ( app->settings.downloadSafeFilename ){
			artist = _nstrdup(song->artist);
			album = _nstrdup(song->album);
			title = _nstrdup(song->title);
		}
		else {
			artist = _slash2dash_strdup(song->artist);
			album = _slash2dash_strdup(song->album);
			title = _slash2dash_strdup(song->title);
		}

		strcpy(songFilename, artist);
		strcat(songFilename, separator);
		strcat(songFilename, album);
		strcat(songFilename, separator);
		strcat(songFilename, title);

		free(artist);
		free(album);
		free(title);
	}

	switch (song->audioFormat) {
#ifdef ENABLE_FAAD
		case PIANO_AF_AACPLUS:
			strcat(songFilename, ".aac");
			break;
#endif
#ifdef ENABLE_MAD
		case PIANO_AF_MP3:
			strcat(songFilename, ".mp3");
			break;
#endif
		default:
			strcat(songFilename, ".dump");
			break;
	}

	strcpy(baseFilename, app->settings.download);
	// TODO Check if trailing slash exists
	strcat(baseFilename, "/");
	io_mkdir(ioq, AT_FDCWD, baseFilename, S_IRWXU | S_IRWXG);

	{
		char *station_ = 0;
		if ( app->settings.downloadSafeFilename ){
			station_ = _nstrdup( station->name );
		}
		else {
			station_ = _slash2dash_strdup( station->name );
		}
		strcat( baseFilename, station_ );
		free( station_ );
	}
	strcat(baseFilename, "/");
	io_mkdir(ioq, AT_FDCWD, baseFilename, S_IRWXU | S_IRWXG);

	/* Loved filename */
	strcpy( download->lovedFilename, baseFilename );
	strcat( download->lovedFilename, songFilename );

	/* Unloved filename */
	strcpy( download->unlovedFilename, baseFilename );
	strcat( download->unlovedFilename, "/unloved/" );
	io_mkdir(ioq, AT_FDCWD, download->unlovedFilename, S_IRWXU | S_IRWXG);
	strcat( download->unlovedFilename, songFilename );

	/* Downloading filename */
	strcpy( download->downloadingFilename, baseFilename );
	strcat( download->downloadingFilename, ".downloading-" );
	strcat( download->downloadingFilename, songFilename );
}

void BarDownloadWrite(struct audioPlayer *player, const void *data, size_t size)
{
	struct io_queue *ioq = &player_to_app(player)->download.io_ctx;
	io_write(ioq, CURR_FD, data, size);
}

static size_t high_watermark;
void BarDownloadStart(BarApp_t *app)
{
	BarDownload_t *d = &app->download;

	if (!app->settings.download)
		/* No download directory set, so return */
		return;

	BarDownloadFilename(app);
	io_open(&d->io_ctx, AT_FDCWD, d->downloadingFilename, O_WRONLY, 0666);
}

void BarDownloadCleanup(BarApp_t *app)
{
	/* Called when we're uncontrollably dying. */
	if (!app)
		return;

	BarDownload_t *d = &app->download;
	pthread_cancel(d->io_ctx.thread);
	if (app->settings.downloadCleanup)
		unlink(d->downloadingFilename);
}

static void ioq_dinit(struct io_queue *ioq)
{
	pthread_cond_destroy(&ioq->cond);
	pthread_mutex_destroy(&ioq->mutex);
}

void BarDownloadFinish(struct audioPlayer *player, WaitressReturn_t wRet) {
	BarApp_t *app = player_to_app(player);
	BarDownload_t *d = &app->download;
	struct io_queue *ioq = &d->io_ctx;

	if (wRet == WAITRESS_RET_OK && !player->songIsAd) {
		// Only "commit" download if everything downloaded okay
		if (app->playlist->rating == PIANO_RATE_LOVE)
			io_rename(ioq, AT_FDCWD, d->downloadingFilename, AT_FDCWD, d->lovedFilename);
		else
			io_rename(ioq, AT_FDCWD, d->downloadingFilename, AT_FDCWD, d->unlovedFilename);
	} else if (player->settings->downloadCleanup)
		io_unlink(ioq, AT_FDCWD, d->downloadingFilename, 0);

	io_close(ioq, CURR_FD);
}

void BarDownloadInit(BarApp_t *app)
{
	/* Called when we start up, setup the io thread */
	struct io_queue *ioq = &app->download.io_ctx;
	ioq_init(ioq);
}

void BarDownloadDini(BarApp_t *app)
{
	/* Called when pianobar is exiting, shutdown the io thread */
	struct io_queue *ioq = &app->download.io_ctx;
	ioq_join(ioq);
	ioq_dinit(ioq);
}
