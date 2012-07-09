/*
Copyright (c) 2008-2012
	Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdbool.h>

#include <piano.h>
#include <waitress.h>
#include <stdbool.h>

/* update structure in ui_dispatch.h if you add shortcuts here */
typedef enum {
	BAR_KS_HELP,
	BAR_KS_LOVE,
	BAR_KS_BAN,
	BAR_KS_ADDMUSIC,
	BAR_KS_CREATESTATION,
	BAR_KS_DELETESTATION,
	BAR_KS_EXPLAIN,
	BAR_KS_GENRESTATION,
	BAR_KS_HISTORY,
	BAR_KS_INFO,
	BAR_KS_ADDSHARED,
	BAR_KS_SKIP,
	BAR_KS_PLAYPAUSE,
	BAR_KS_QUIT,
	BAR_KS_RENAMESTATION,
	BAR_KS_SELECTSTATION,
	BAR_KS_TIRED,
	BAR_KS_UPCOMING,
	BAR_KS_SELECTQUICKMIX,
	BAR_KS_DEBUG,
	BAR_KS_BOOKMARK,
	BAR_KS_VOLDOWN,
	BAR_KS_VOLUP,
	BAR_KS_MANAGESTATION,
	BAR_KS_PLAYPAUSE2,
	BAR_KS_CREATESTATIONFROMSONG,
    BAR_KS_GAINTOGGLE,
	BAR_KS_VOLMUTE,
	/* insert new shortcuts _before_ this element */
	BAR_KS_COUNT,
} BarKeyShortcutId_t;

#define BAR_KS_DISABLED '\x00'

typedef enum {
	BAR_SORT_NAME_AZ = 0,
	BAR_SORT_NAME_ZA = 1,
	BAR_SORT_QUICKMIX_01_NAME_AZ = 2,
	BAR_SORT_QUICKMIX_01_NAME_ZA = 3,
	BAR_SORT_QUICKMIX_10_NAME_AZ = 4,
	BAR_SORT_QUICKMIX_10_NAME_ZA = 5,
	BAR_SORT_COUNT = 6,
} BarStationSorting_t;

#include "ui_types.h"

typedef struct {
	char *prefix;
	char *postfix;
} BarMsgFormatStr_t;

typedef struct {
	bool autoselect;
	unsigned int history;
	int volume;
	bool mute;
	bool noReplayGain;
	BarStationSorting_t sortOrder;
	PianoAudioQuality_t audioQuality;
	char *username;
	char *password;
	char *controlProxy; /* non-american listeners need this */
	char *proxy;
	char *autostartStation;
	char *eventCmd;
	char *loveIcon;
	char *banIcon;
	char *atIcon;
	char *npSongFormat;
	char *npStationFormat;
	char *listSongFormat;
	char *fifo;
	char *rpcHost, *partnerUser, *partnerPassword, *device, *inkey, *outkey;
	char tlsFingerprint[20];
	char keys[BAR_KS_COUNT];
	BarMsgFormatStr_t msgFormat[MSG_COUNT];
	char *download;
	bool downloadSafeFilename;
	char *downloadSeparator;
    bool downloadCleanup;
} BarSettings_t;

void BarSettingsInit (BarSettings_t *);
void BarSettingsDestroy (BarSettings_t *);
void BarSettingsRead (BarSettings_t *);
void BarGetXdgConfigDir (const char *, char *, size_t);

#endif /* _SETTINGS_H */
