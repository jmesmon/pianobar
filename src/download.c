#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "download.h"

static void BarDownloadFilename(BarApp_t *app) {
	char baseFilename[1024 * 2];
	char songFilename[1024 * 2];

    memset(songFilename, 0, sizeof (songFilename));
    memset(baseFilename, 0, sizeof (baseFilename));

    {
	    char *artist = 0, *album = 0, *title = 0, *next_slash = 0;

        artist = strdup(app->playlist->artist);
        album = strdup(app->playlist->album);
        title = strdup(app->playlist->title);

        next_slash = strchr(artist, '/');
        while (next_slash != NULL) {
            *next_slash = '-';
            next_slash = strchr(artist, '/');
        }

        next_slash = strchr(album, '/');
        while (next_slash != NULL) {
            *next_slash = '-';
            next_slash = strchr(album, '/');
        }

        next_slash = strchr(title, '/');
        while (next_slash != NULL) {
            *next_slash = '-';
            next_slash = strchr(title, '/');
        }

        strcpy(songFilename, artist);
        strcat(songFilename, "-");
        strcat(songFilename, album);
        strcat(songFilename, "-");
        strcat(songFilename, title);

        switch (app->playlist->audioFormat) {
            #ifdef ENABLE_FAAD
            case PIANO_AF_AACPLUS:
                strcat(songFilename, ".aac");
                break;
            #endif
            #ifdef ENABLE_MAD
            case PIANO_AF_MP3:
            case PIANO_AF_MP3_HI:
                strcat(songFilename, ".mp3");
                break;
            #endif
            default:
                strcat(songFilename, ".dump");
                break;
        }

        free(artist);
        free(album);
        free(title);
    }

    /*strcpy(filename, "dumps/");*/
    /*strcpy(filename, "./pianobar-download/");*/
    strcpy(baseFilename, app->settings.download);
    // TODO Check if trailing slash exists
	strcat(baseFilename, "/");
	mkdir(baseFilename, S_IRWXU | S_IRWXG);

	strcat(baseFilename, app->curStation->name);
	strcat(baseFilename, "/");
	mkdir(baseFilename, S_IRWXU | S_IRWXG);

    /* Loved filename */
    strcpy( app->player.loveFilename, baseFilename );
    strcat( app->player.loveFilename, songFilename );

    /* Unloved filename */
    strcpy( app->player.unloveFilename, baseFilename );
    strcat( app->player.unloveFilename, "/unloved/" );
	mkdir( app->player.unloveFilename, S_IRWXU | S_IRWXG);
    strcat( app->player.unloveFilename, songFilename );

    /* Downloading filename */
    strcpy( app->player.downloadFilename, baseFilename );
    strcat( app->player.downloadFilename, ".downloading-" );
    strcat( app->player.downloadFilename, songFilename );
}

void BarDownloadStart(BarApp_t *app) {

    /* Clear out/reset the download structure in preparation of downloading a new song */
    memset(app->player.downloadFilename, 0, sizeof (app->player.downloadFilename));
    memset(app->player.loveFilename, 0, sizeof (app->player.loveFilename));
    memset(app->player.unloveFilename, 0, sizeof (app->player.unloveFilename));

    app->player.downloadHandle = NULL;

    /* Indicate that the song is loved so we save it to the right place */
    app->player.loveSong = app->playlist->rating == PIANO_RATE_LOVE ? 1 : 0;

    if (! app->settings.download) {
        /* No download directory set, so return */
        return;
    }

    BarDownloadFilename(app);

    if (access(app->player.downloadFilename, R_OK) != 0) {
        app->player.downloadHandle = fopen(app->player.downloadFilename, "w");
    } else {
        app->player.downloadHandle = NULL;
    }
}

void BarDownloadWrite(struct audioPlayer *player, char *data, size_t size) {

	if (player->downloadHandle != NULL) {
		fwrite(data, size, 1, player->downloadHandle);
	}

}

void BarDownloadFinish(struct audioPlayer *player, WaitressReturn_t wRet) {

    if (player->downloadHandle!= NULL) {
        fclose(player->downloadHandle);
        player->downloadHandle = NULL;
        if (wRet == WAITRESS_RET_OK) {
            // Only "commit" download if everything downloaded okay
            // TODO: Cleanup of partial files?
            if (player->loveSong) {
                rename(player->downloadFilename, player->loveFilename);
            }
            else {
                rename(player->downloadFilename, player->unloveFilename);
            }
        }
    }

}
