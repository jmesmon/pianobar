#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "download.h"

void BarDownloadPrepareFilename(BarApp_t *app) {
	char baseFilename[1024 * 2];
	char songFilename[1024 * 2];

    memset(app->player.downloadFilename, 0, sizeof (app->player.downloadFilename));
    memset(app->player.loveFilename, 0, sizeof (app->player.loveFilename));
    memset(app->player.unloveFilename, 0, sizeof (app->player.unloveFilename));

    if (! app->settings.download) {
        return;
    }

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
