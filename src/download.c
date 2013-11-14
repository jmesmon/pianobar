#ifndef __FreeBSD__
#define _POSIX_C_SOURCE 1 /* fileno() */
#define _BSD_SOURCE /* strdup() */
#define _DARWIN_C_SOURCE /* strdup() on OS X */
#endif

#include <string.h> /* strdup() */
#include <sys/stat.h> /* mkdir() */

#include "download.h"
#include "ui.h" /* BarUiMsg */

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

void BarDownloadWrite(struct audioPlayer *player, const void *data, size_t size) {
	if (player->download.handle != NULL) {
		fwrite(data, size, 1, player->download.handle);
	}
}

void BarDownloadFinish(struct audioPlayer *player, WaitressReturn_t wRet) {
	if (player->download.handle!= NULL) {
		fclose(player->download.handle);
		player->download.handle = NULL;
		if (wRet == WAITRESS_RET_OK) {
			// Only "commit" download if everything downloaded okay
			if (player->download.loveSong) {
				rename(player->download.downloadingFilename, player->download.lovedFilename);
			} else {
				rename(player->download.downloadingFilename, player->download.unlovedFilename);
			}
		} else {
			if (player->download.cleanup) {
				unlink(player->download.downloadingFilename);
			}
		}
	}
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

void BarDownloadStart(BarApp_t *app) {

    /* Indicate that the song is loved so we save it to the right place */
    app->player.download.loveSong = app->playlist->rating == PIANO_RATE_LOVE ? 1 : 0;

    /* Pass through the cleanup setting */
    app->player.download.cleanup = app->settings.downloadCleanup;

    if (! app->settings.download) {
        /* No download directory set, so return */
	BarUiMsg (&app->settings, MSG_ERR,
			"Error: Download directory not set\n");
        return;
    }

    BarDownloadFilename(app);

    if (access(app->player.download.downloadingFilename, R_OK) != 0) {
        app->player.download.handle = fopen(app->player.download.downloadingFilename, "w");
    } else {
        app->player.download.handle = NULL;
    }
}

