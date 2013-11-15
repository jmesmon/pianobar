#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <string.h> /* strdup() */
#include <sys/stat.h> /* mkdir() */

#include "download.h"
#include "ui.h" /* BarUiMsg */
#include "circ_buf.h"

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
	BarDownload_t *download = &(app->player.download);

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
	mkdir(baseFilename, S_IRWXU | S_IRWXG);

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
	mkdir(baseFilename, S_IRWXU | S_IRWXG);

	/* Loved filename */
	strcpy( download->lovedFilename, baseFilename );
	strcat( download->lovedFilename, songFilename );

	/* Unloved filename */
	strcpy( download->unlovedFilename, baseFilename );
	strcat( download->unlovedFilename, "/unloved/" );
	mkdir( download->unlovedFilename, S_IRWXU | S_IRWXG);
	strcat( download->unlovedFilename, songFilename );

	/* Downloading filename */
	strcpy( download->downloadingFilename, baseFilename );
	strcat( download->downloadingFilename, ".downloading-" );
	strcat( download->downloadingFilename, songFilename );
}

bool is_downloading(BarDownload_t *d)
{
	/* TODO: check on io_thread */
	return d->handle != NULL;
}

void *memdup(const void *data, size_t len)
{
	void *x = malloc(len);
	if (!x)
		return NULL;

	memcpy(x, data, len);
	return x;
}

void BarDownloadWrite(struct audioPlayer *player, const void *data, size_t size)
{
	BarDownload_t *d = &player->download;
	if (!is_downloading(d))
		return;

	struct io_queue *ioq = &d->io_ctx;
	pthread_mutex_lock(&ioq->mutex);
	size_t curr = CIRC_CNT(ioq->head, ioq->tail, IOP_CT);
	if (curr > ioq->high)
		ioq->high = curr;
	while (CIRC_FULL(ioq->head, ioq->tail, IOP_CT)) {
		fprintf(stderr, "CIRC_FULL head: %zu tail: %zu\n", ioq->head, ioq->tail);
		pthread_cond_wait(&ioq->cond, &ioq->mutex);
	}

	struct io_op *iop = &ioq->iops[ioq->head];
	iop->type = IO_TYPE_WRITE;
	iop->data.write.data = memdup(data, size);
	iop->data.write.len  = size;

	if (CIRC_EMPTY(ioq->head, ioq->tail, IOP_CT))
		pthread_cond_signal(&ioq->cond);
	ioq->head = CIRC_NEXT(ioq->head, IOP_CT);
	pthread_mutex_unlock(&ioq->mutex);
}

static void io_queue_join(struct io_queue *ioq)
{
	pthread_mutex_lock(&ioq->mutex);
	while (CIRC_FULL(ioq->head, ioq->tail, IOP_CT))
		pthread_cond_wait(&ioq->cond, &ioq->mutex);
	struct io_op *iop = &ioq->iops[ioq->head];
	iop->type = IO_TYPE_EXIT;
	if (CIRC_EMPTY(ioq->head, ioq->tail, IOP_CT))
		pthread_cond_signal(&ioq->cond);
	ioq->head = CIRC_NEXT(ioq->head, IOP_CT);
	pthread_mutex_unlock(&ioq->mutex);
	pthread_join(ioq->thread, NULL);
}

static size_t high_watermark;
void BarDownloadFinish(struct audioPlayer *player, WaitressReturn_t wRet) {
	BarDownload_t *d = &player->download;
	if (!is_downloading(d))
		return;

	/* Stop the io_thread */
	io_queue_join(&d->io_ctx);

	fclose(d->handle);
	d->handle = NULL;

	if (d->io_ctx.high > high_watermark)
		high_watermark = d->io_ctx.high;

	if (wRet == WAITRESS_RET_OK) {
		// Only "commit" download if everything downloaded okay
		if (d->loveSong)
			rename(d->downloadingFilename, d->lovedFilename);
		else
			rename(d->downloadingFilename, d->unlovedFilename);
	} else if (d->cleanup)
		unlink(d->downloadingFilename);
}

void *io_thread(void *v) {
	BarApp_t *app = v;
	BarDownload_t *d = &app->player.download;
	struct io_queue *io = &d->io_ctx;

	for (;;) {
		pthread_mutex_lock(&io->mutex);
		while (CIRC_EMPTY(io->head, io->tail, IOP_CT))
			pthread_cond_wait(&io->cond, &io->mutex);

		struct io_op *iop = &io->iops[io->tail];
		switch(iop->type) {
			case IO_TYPE_EXIT:
				if (CIRC_FULL(io->head, io->tail, IOP_CT))
					pthread_cond_signal(&io->cond);
				io->tail = CIRC_NEXT(io->tail, IOP_CT);
				pthread_mutex_unlock(&io->mutex);
				return NULL;
			case IO_TYPE_WRITE:
				fwrite(iop->data.write.data, iop->data.write.len, 1, d->handle);
				free(iop->data.write.data);
				break;
			default:
				/* racy, but we don't care */
				BarUiMsg(&app->settings, MSG_ERR, "io_thread failure");
				abort();
		}

		if (CIRC_FULL(io->head, io->tail, IOP_CT))
			pthread_cond_signal(&io->cond);

		io->tail = CIRC_NEXT(io->tail, IOP_CT);
		pthread_mutex_unlock(&io->mutex);
	}
}

void BarDownloadStart(BarApp_t *app) {
	BarDownload_t *d = &app->player.download;

	/* Indicate that the song is loved so we save it to the right place */
	/* XXX: this can change at _any_ point while we're playing */
	d->loveSong = app->playlist->rating == PIANO_RATE_LOVE ? 1 : 0;

	/* Pass through the cleanup setting */
	d->cleanup = app->settings.downloadCleanup;

	if (!app->settings.download)
		/* No download directory set, so return */
		return;

	BarDownloadFilename(app);

	d->handle = fopen(d->downloadingFilename, "w");
	if (!d->handle)
		BarUiMsg (&app->settings, MSG_ERR, "Could not open file %s to save to",
				d->downloadingFilename);

	pthread_mutex_init(&d->io_ctx.mutex, NULL);
	pthread_cond_init(&d->io_ctx.cond, NULL);
	pthread_create(&d->io_ctx.thread, NULL, io_thread, app);
	BarUiMsg(&app->settings, MSG_ERR, "high watermark: %zu\n", high_watermark);
}

void BarDownloadCleanup(BarApp_t *app)
{
	if (!app)
		return;

	BarDownload_t *d = &app->player.download;
	pthread_cancel(d->io_ctx.thread);
	if (d->cleanup)
		unlink(d->downloadingFilename);
}

void BarDownloadInit(BarApp_t *app)
{
	/* TODO: set up io context to avoid blocking between songs */
}

void BarDownloadDini(BarApp_t *app)
{
}
