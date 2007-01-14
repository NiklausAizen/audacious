/*  Audtool -- Audacious scripting tool
 *  Copyright (c) 2005-2007  Audacious development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <locale.h>
#include "libaudacious/beepctrl.h"
#include "audtool.h"

struct commandhandler handlers[] = {
	{"<sep>", NULL, "Vital information"},
	{"current-song", get_current_song, "returns current song title"},
	{"current-song-filename", get_current_song_filename, "returns current song filename"},
	{"current-song-length", get_current_song_length, "returns current song length"},
	{"current-song-length-seconds", get_current_song_length_seconds, "returns current song length in seconds"},
	{"current-song-length-frames", get_current_song_length_frames, "returns current song length in frames"},
	{"current-song-output-length", get_current_song_output_length, "returns current song output length"},
	{"current-song-output-length-seconds", get_current_song_output_length_seconds, "returns current song output length in seconds"},
	{"current-song-output-length-frames", get_current_song_output_length_frames, "returns current song output length in frames"},
	{"current-song-bitrate", get_current_song_bitrate, "returns current song bitrate in bits per second"},
	{"current-song-bitrate-kbps", get_current_song_bitrate_kbps, "returns current song bitrate in kilobits per second"},
	{"current-song-frequency", get_current_song_frequency, "returns current song frequency in hertz"},
	{"current-song-frequency-khz", get_current_song_frequency_khz, "returns current song frequency in kilohertz"},
	{"current-song-channels", get_current_song_channels, "returns current song channels"},
	{"<sep>", NULL, "Playlist manipulation"},
	{"playlist-advance", playlist_advance, "go to the next song in the playlist"},
	{"playlist-reverse", playlist_reverse, "go to the previous song in the playlist"},
	{"playlist-addurl", playlist_add_url_string, "adds a url to the playlist"},
	{"playlist-delete", playlist_delete, "deletes a song from the playlist"},
	{"playlist-length", playlist_length, "returns the total length of the playlist"},
	{"playlist-song", playlist_song, "returns the title of a song in the playlist"},
	{"playlist-song-filename", playlist_song_filename, "returns the filename of a song in the playlist"},
	{"playlist-song-length", playlist_song_length, "returns the length of a song in the playlist"},
	{"playlist-song-length-seconds", playlist_song_length_seconds, "returns the length of a song in the playlist in seconds"},
	{"playlist-song-length-frames", playlist_song_length_frames, "returns the length of a song in the playlist in frames"},
	{"playlist-display", playlist_display, "returns the entire playlist"},
	{"playlist-position", playlist_position, "returns the position in the playlist"},
	{"playlist-jump", playlist_jump, "jumps to a position in the playlist"},
	{"playlist-clear", playlist_clear, "clears the playlist"},
	{"playlist-repeat-status", playlist_repeat_status, "returns the status of playlist repeat"},
	{"playlist-repeat-toggle", playlist_repeat_toggle, "toggles playlist repeat"},
	{"playlist-shuffle-status", playlist_shuffle_status, "returns the status of playlist shuffle"},
	{"playlist-shuffle-toggle", playlist_shuffle_toggle, "toggles playlist shuffle"},
	{"<sep>", NULL, "Playqueue manipulation"},
	{"playqueue-add", playqueue_add, "adds a song to the playqueue"},
	{"playqueue-remove", playqueue_remove, "removes a song from the playqueue"},
	{"playqueue-is-queued", playqueue_is_queued, "returns OK if a song is queued"},
	{"playqueue-get-position", playqueue_get_position, "returns the queue position of a song in the playlist"},
	{"playqueue-get-qposition", playqueue_get_qposition, "returns the playlist position of a song in the queue"},
	{"playqueue-length", playqueue_length, "returns the length of the playqueue"},
	{"playqueue-display", playqueue_display, "returns a list of currently-queued songs"},
	{"playqueue-clear", playqueue_clear, "clears the playqueue"},
	{"<sep>", NULL, "Playback manipulation"},
	{"playback-play", playback_play, "starts/unpauses song playback"},
	{"playback-pause", playback_pause, "(un)pauses song playback"},
	{"playback-playpause", playback_playpause, "plays/(un)pauses song playback"},
	{"playback-stop", playback_stop, "stops song playback"},
	{"playback-playing", playback_playing, "returns OK if audacious is playing"},
	{"playback-paused", playback_paused, "returns OK if audacious is paused"},
	{"playback-stopped", playback_stopped, "returns OK if audacious is stopped"},
	{"playback-status", playback_status, "returns the playback status"},
	{"playback-seek", playback_seek, "performs an absolute seek"},
	{"playback-seek-relative", playback_seek_relative, "performs a seek relative to the current position"},
	{"<sep>", NULL, "Volume control"},
	{"get-volume", get_volume, "returns the current volume level in percent"},
	{"set-volume", set_volume, "sets the current volume level in percent"},
	{"<sep>", NULL, "Miscellaneous"},
	{"mainwin-show", mainwin_show, "shows/hides the main window"},
	{"playlist-show", playlist_show, "shows/hides the playlist window"},
	{"equalizer-show", equalizer_show, "shows/hides the equalizer window"},
	{"preferences", show_preferences_window, "shows/hides the preferences window"},
	{"jumptofile", show_jtf_window, "shows the jump to file window"},
	{"shutdown", shutdown_audacious_server, "shuts down audacious"},
	{"<sep>", NULL, "Help system"},
	{"list-handlers", get_handlers_list, "shows handlers list"},
	{"help", get_handlers_list, "shows handlers list"},
	{NULL, NULL, NULL}
};

gint main(gint argc, gchar **argv)
{
	gint i;
	gchar *remote_uri;

	setlocale(LC_CTYPE, "");

	if (argc < 2)
	{
		g_print("%s: usage: %s <command>\n", argv[0], argv[0]);
		g_print("%s: use `%s help' to get a listing of available commands.\n",
			argv[0], argv[0]);
		exit(-2);
	}

	remote_uri = getenv("AUDTOOL_REMOTE_URI");
	audacious_set_session_uri(remote_uri);

	if (!xmms_remote_is_running(0) && g_strcasecmp("help", argv[1])
		&& g_strcasecmp("list-handlers", argv[1]))
	{
		g_print("%s: audacious server is not running!\n", argv[0]);
		exit(-1);
	}

	for (i = 0; handlers[i].name != NULL; i++)
	{
		if ((!g_strcasecmp(handlers[i].name, argv[1]) ||
		     !g_strcasecmp(g_strconcat("--", handlers[i].name, NULL), argv[1]))
		    && g_strcasecmp("<sep>", handlers[i].name))
  		{
 			handlers[i].handler(0, argc, argv);
			exit(0);
		}
	}

	g_print("%s: invalid command '%s'\n", argv[0], argv[1]);
	g_print("%s: use `%s help' to get a listing of available commands.\n", argv[0], argv[0]);

	return 0;
}

/*** MOVE TO HANDLERS.C ***/

void get_current_song(gint session, gint argc, gchar **argv)
{
	gint playpos = xmms_remote_get_playlist_pos(session);
	gchar *song = xmms_remote_get_playlist_title(session, playpos);

	if (!song)
	{
		g_print("No song playing.\n");
		return;
	}

	g_print("%s\n", song);
}

void get_current_song_filename(gint session, gint argc, gchar **argv)
{
	gint playpos = xmms_remote_get_playlist_pos(session);

	g_print("%s\n", xmms_remote_get_playlist_file(session, playpos));
}

void get_current_song_output_length(gint session, gint argc, gchar **argv)
{
	gint frames = xmms_remote_get_output_time(session);
	gint length = frames / 1000;

	g_print("%d:%.2d\n", length / 60, length % 60);
}

void get_current_song_output_length_seconds(gint session, gint argc, gchar **argv)
{
	gint frames = xmms_remote_get_output_time(session);
	gint length = frames / 1000;

	g_print("%d\n", length);
}

void get_current_song_output_length_frames(gint session, gint argc, gchar **argv)
{
	gint frames = xmms_remote_get_output_time(session);

	g_print("%d\n", frames);
}

void get_current_song_length(gint session, gint argc, gchar **argv)
{
	gint playpos = xmms_remote_get_playlist_pos(session);
	gint frames = xmms_remote_get_playlist_time(session, playpos);
	gint length = frames / 1000;

	g_print("%d:%.2d\n", length / 60, length % 60);
}

void get_current_song_length_seconds(gint session, gint argc, gchar **argv)
{
	gint playpos = xmms_remote_get_playlist_pos(session);
	gint frames = xmms_remote_get_playlist_time(session, playpos);
	gint length = frames / 1000;

	g_print("%d\n", length);
}

void get_current_song_length_frames(gint session, gint argc, gchar **argv)
{
	gint playpos = xmms_remote_get_playlist_pos(session);
	gint frames = xmms_remote_get_playlist_time(session, playpos);

	g_print("%d\n", frames);
}

void get_current_song_bitrate(gint session, gint argc, gchar **argv)
{
	gint rate, freq, nch;

	xmms_remote_get_info(session, &rate, &freq, &nch);

	g_print("%d\n", rate);
}

void get_current_song_bitrate_kbps(gint session, gint argc, gchar **argv)
{
	gint rate, freq, nch;

	xmms_remote_get_info(session, &rate, &freq, &nch);

	g_print("%d\n", rate / 1000);
}

void get_current_song_frequency(gint session, gint argc, gchar **argv)
{
	gint rate, freq, nch;

	xmms_remote_get_info(session, &rate, &freq, &nch);

	g_print("%d\n", freq);
}

void get_current_song_frequency_khz(gint session, gint argc, gchar **argv)
{
	gint rate, freq, nch;

	xmms_remote_get_info(session, &rate, &freq, &nch);

	g_print("%0.1f\n", (gfloat) freq / 1000);
}

void get_current_song_channels(gint session, gint argc, gchar **argv)
{
	gint rate, freq, nch;

	xmms_remote_get_info(session, &rate, &freq, &nch);

	g_print("%d\n", nch);
}

void playlist_reverse(gint session, gint argc, gchar **argv)
{
	xmms_remote_playlist_prev(session);
}

void playlist_advance(gint session, gint argc, gchar **argv)
{
	xmms_remote_playlist_next(session);
}

void playback_play(gint session, gint argc, gchar **argv)
{
	xmms_remote_play(session);
}

void playback_pause(gint session, gint argc, gchar **argv)
{
	xmms_remote_pause(session);
}

void playback_playpause(gint session, gint argc, gchar **argv)
{
	if (xmms_remote_is_playing(session))
	{
		xmms_remote_pause(session);
	}
	else
	{
		xmms_remote_play(session);
	}
}

void playback_stop(gint session, gint argc, gchar **argv)
{
	xmms_remote_stop(session);
}

void playback_playing(gint session, gint argc, gchar **argv)
{
	if (!xmms_remote_is_paused(session))
	{
		exit(!xmms_remote_is_playing(session));
	}
	else
	{
		exit(1);
	}
}

void playback_paused(gint session, gint argc, gchar **argv)
{
	exit(!xmms_remote_is_paused(session));
}

void playback_stopped(gint session, gint argc, gchar **argv)
{
	if (!xmms_remote_is_playing(session) && !xmms_remote_is_paused(session))
	{
		exit(0);
	}
	else
	{
		exit(1);
	}
}

void playback_status(gint session, gint argc, gchar **argv)
{
	if (xmms_remote_is_paused(session))
	{
		g_print("paused\n");
		return;
	}
	else if (xmms_remote_is_playing(session))
	{
		g_print("playing\n");
		return;
	}
	else
	{
		g_print("stopped\n");
		return;
	}
}

void playback_seek(gint session, gint argc, gchar **argv)
{
	if (argc < 3)
	{
		g_print("%s: invalid parameters for playback-seek.\n", argv[0]);
		g_print("%s: syntax: %s playback-seek <position>\n", argv[0], argv[0]);
		return;
	}

	xmms_remote_jump_to_time(session, atoi(argv[2]) * 1000);
}

void playback_seek_relative(gint session, gint argc, gchar **argv)
{
	gint oldtime, newtime, diff;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playback-seek-relative.\n", argv[0]);
		g_print("%s: syntax: %s playback-seek <position>\n", argv[0], argv[0]);
		return;
	}

	oldtime = xmms_remote_get_output_time(session);
	diff = atoi(argv[2]) * 1000;
	newtime = oldtime + diff;

	xmms_remote_jump_to_time(session, newtime);
}

void playlist_add_url_string(gint session, gint argc, gchar **argv)
{
	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-addurl.\n", argv[0]);
		g_print("%s: syntax: %s playlist-addurl <url>\n", argv[0], argv[0]);
		return;
	}

	xmms_remote_playlist_add_url_string(session, argv[2]);
}

void playlist_delete(gint session, gint argc, gchar **argv)
{
	gint playpos;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-delete.\n", argv[0]);
		g_print("%s: syntax: %s playlist-delete <position>\n", argv[0], argv[0]);
		return;
	}

	playpos = atoi(argv[2]);

	if (playpos < 1 || playpos > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], playpos);
		return;
	}

	xmms_remote_playlist_delete(session, playpos - 1);
}

void playlist_length(gint session, gint argc, gchar **argv)
{
	gint i;

	i = xmms_remote_get_playlist_length(session);

	g_print("%d\n", i);
}

void playlist_song(gint session, gint argc, gchar **argv)
{
	gint playpos;
	gchar *song;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-song-title.\n", argv[0]);
		g_print("%s: syntax: %s playlist-song-title <position>\n", argv[0], argv[0]);
		return;
	}

	playpos = atoi(argv[2]);

	if (playpos < 1 || playpos > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], playpos);
		return;
	}

	song = xmms_remote_get_playlist_title(session, playpos - 1);

	g_print("%s\n", song);
}


void playlist_song_length(gint session, gint argc, gchar **argv)
{
	gint playpos, frames, length;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-song-length.\n", argv[0]);
		g_print("%s: syntax: %s playlist-song-length <position>\n", argv[0], argv[0]);
		return;
	}

	playpos = atoi(argv[2]);

	if (playpos < 1 || playpos > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], playpos);
		return;
	}

	frames = xmms_remote_get_playlist_time(session, playpos - 1);
	length = frames / 1000;

	g_print("%d:%.2d\n", length / 60, length % 60);
}

void playlist_song_length_seconds(gint session, gint argc, gchar **argv)
{
	gint playpos, frames, length;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-song-length-seconds.\n", argv[0]);
		g_print("%s: syntax: %s playlist-song-length-seconds <position>\n", argv[0], argv[0]);
		return;
	}

	playpos = atoi(argv[2]);

	if (playpos < 1 || playpos > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], playpos);
		return;
	}

	frames = xmms_remote_get_playlist_time(session, playpos - 1);
	length = frames / 1000;

	g_print("%d\n", length);
}

void playlist_song_length_frames(gint session, gint argc, gchar **argv)
{
	gint playpos, frames;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-song-length-frames.\n", argv[0]);
		g_print("%s: syntax: %s playlist-song-length-frames <position>\n", argv[0], argv[0]);
		return;
	}

	playpos = atoi(argv[2]);

	if (playpos < 1 || playpos > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], playpos);
		return;
	}

	frames = xmms_remote_get_playlist_time(session, playpos - 1);

	g_print("%d\n", frames);
}

void playlist_display(gint session, gint argc, gchar **argv)
{
	gint i, ii, frames, length, total;
	gchar *songname;
	gchar *fmt = NULL, *p;
	gint column;

	i = xmms_remote_get_playlist_length(session);

	g_print("%d tracks.\n", i);

	total = 0;

	for (ii = 0; ii < i; ii++)
	{
		songname = xmms_remote_get_playlist_title(session, ii);
		frames = xmms_remote_get_playlist_time(session, ii);
		length = frames / 1000;
		total += length;

		/* adjust width for multi byte characters */
		column = 60;
		if(songname){
			p = songname;
			while(*p){
				gint stride;
				stride = g_utf8_next_char(p) - p;
				if(g_unichar_iswide(g_utf8_get_char(p))
#if ( (GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION >= 12) )
				   || g_unichar_iswide_cjk(g_utf8_get_char(p))
#endif
                                ){
					column += (stride - 2);
				}
				else {
					column += (stride - 1);
				}
				p = g_utf8_next_char(p);
			}

		}

		fmt = g_strdup_printf("%%4d | %%-%ds | %%d:%%.2d\n", column);
		g_print(fmt, ii + 1, songname, length / 60, length % 60);
		g_free(fmt);
	}

	g_print("Total length: %d:%.2d\n", total / 60, total % 60);
}

void playlist_position(gint session, gint argc, gchar **argv)
{
	gint i;

	i = xmms_remote_get_playlist_pos(session);

	g_print("%d\n", i + 1);
}

void playlist_song_filename(gint session, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-filename.\n", argv[0]);
		g_print("%s: syntax: %s playlist-filename <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	g_print("%s\n", xmms_remote_get_playlist_file(session, i - 1));
}

void playlist_jump(gint session, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playlist-jump.\n", argv[0]);
		g_print("%s: syntax: %s playlist-jump <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	xmms_remote_set_playlist_pos(session, i - 1);
}

void playlist_clear(gint session, gint argc, gchar **argv)
{
	xmms_remote_playlist_clear(session);
}

void playlist_repeat_status(gint session, gint argc, gchar **argv)
{
	if (xmms_remote_is_repeat(session))
	{
		g_print("on\n");
		return;
	}
	else
	{
		g_print("off\n");
		return;
	}
}

void playlist_repeat_toggle(gint session, gint argc, gchar **argv)
{
	xmms_remote_toggle_repeat(session);
}

void playlist_shuffle_status(gint session, gint argc, gchar **argv)
{
	if (xmms_remote_is_shuffle(session))
	{
		g_print("on\n");
		return;
	}
	else
	{
		g_print("off\n");
		return;
	}
}

void playlist_shuffle_toggle(gint session, gint argc, gchar **argv)
{
	xmms_remote_toggle_shuffle(session);
}

void playqueue_add(gint session, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playqueue-add.\n", argv[0]);
		g_print("%s: syntax: %s playqueue-add <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	if (!(xmms_remote_playqueue_is_queued(session, i - 1)))
		xmms_remote_playqueue_add(session, i - 1);
}

void playqueue_remove(gint session, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playqueue-remove.\n", argv[0]);
		g_print("%s: syntax: %s playqueue-remove <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	if (xmms_remote_playqueue_is_queued(session, i - 1))
		xmms_remote_playqueue_remove(session, i - 1);
}

void playqueue_is_queued(gint session, gint argc, gchar **argv)
{
	gint i;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playqueue-is-queued.\n", argv[0]);
		g_print("%s: syntax: %s playqueue-is-queued <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	exit(!(xmms_remote_playqueue_is_queued(session, i - 1)));
}

void playqueue_get_position(gint session, gint argc, gchar **argv)
{
	gint i, pos;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playqueue-get-position.\n", argv[0]);
		g_print("%s: syntax: %s playqueue-get-position <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playlist_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	pos = xmms_remote_get_playqueue_position(session, i - 1) + 1;

	if (pos < 1)
		return;

	g_print("%d\n", pos);
}

void playqueue_get_qposition(gint session, gint argc, gchar **argv)
{
	gint i, pos;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for playqueue-get-qposition.\n", argv[0]);
		g_print("%s: syntax: %s playqueue-get-qposition <position>\n", argv[0], argv[0]);
		return;
	}

	i = atoi(argv[2]);

	if (i < 1 || i > xmms_remote_get_playqueue_length(session))
	{
		g_print("%s: invalid playlist position %d\n", argv[0], i);
		return;
	}

	pos = xmms_remote_get_playqueue_queue_position(session, i - 1) + 1;

	if (pos < 1)
		return;

	g_print("%d\n", pos);
}

void playqueue_display(gint session, gint argc, gchar **argv)
{
	gint i, ii, position, frames, length, total;
	gchar *songname;
	gchar *fmt = NULL, *p;
	gint column;
	
	i = xmms_remote_get_playqueue_length(session);

	g_print("%d queued tracks.\n", i);

	total = 0;

	for (ii = 0; ii < i; ii++)
	{
		position = xmms_remote_get_playqueue_queue_position(session, ii);
		songname = xmms_remote_get_playlist_title(session, position);
		frames = xmms_remote_get_playlist_time(session, position);
		length = frames / 1000;
		total += length;

		/* adjust width for multi byte characters */
		column = 60;
		if(songname) {
			p = songname;
			while(*p){
				gint stride;
				stride = g_utf8_next_char(p) - p;
				if(g_unichar_iswide(g_utf8_get_char(p))
#if ( (GLIB_MAJOR_VERSION == 2) && (GLIB_MINOR_VERSION >= 12) )
				   || g_unichar_iswide_cjk(g_utf8_get_char(p))
#endif
				){
					column += (stride - 2);
				}
				else {
					column += (stride - 1);
				}
				p = g_utf8_next_char(p);
			}
		}

		fmt = g_strdup_printf("%%4d | %%4d | %%-%ds | %%d:%%.2d\n", column);
		g_print(fmt, ii + 1, position + 1, songname, length / 60, length % 60);
		g_free(fmt);
	}

	g_print("Total length: %d:%.2d\n", total / 60, total % 60);
}

void playqueue_length(gint session, gint argc, gchar **argv)
{
	gint i;

	i = xmms_remote_get_playqueue_length(session);

	g_print("%d\n", i);
}

void playqueue_clear(gint session, gint argc, gchar **argv)
{
	xmms_remote_playqueue_clear(session);
}

void get_volume(gint session, gint argc, gchar **argv)
{
	gint i;

	i = xmms_remote_get_main_volume(session);

	g_print("%d\n", i);
}

void set_volume(gint session, gint argc, gchar **argv)
{
	gint i, current_volume;

	if (argc < 3)
	{
		g_print("%s: invalid parameters for set-volume.\n", argv[0]);
		g_print("%s: syntax: %s set-volume <level>\n", argv[0], argv[0]);
		return;
	}

	current_volume = xmms_remote_get_main_volume(session);
	switch (argv[2][0]) 
	{
		case '+':
		case '-':
			i = current_volume + atoi(argv[2]);
			break;
		default:
			i = atoi(argv[2]);
			break;
	}

	xmms_remote_set_main_volume(session, i);
}

void mainwin_show(gint session, gint argc, gchar **argv)
{
	if (argc > 2)
	{
		if (!strncmp(argv[2],"on",2)) {
			xmms_remote_main_win_toggle(session, TRUE);
			return;
		}
		else if (!strncmp(argv[2],"off",3)) {
			xmms_remote_main_win_toggle(session, FALSE);
			return;
		}
	}
	g_print("%s: invalid parameter for mainwin-show.\n",argv[0]);
	g_print("%s: syntax: %s mainwin-show <on/off>\n",argv[0],argv[0]);
}

void playlist_show(gint session, gint argc, gchar **argv)
{
	if (argc > 2)
	{
		if (!strncmp(argv[2],"on",2)) {
			xmms_remote_pl_win_toggle(session, TRUE);
			return;
		}
		else if (!strncmp(argv[2],"off",3)) {
			xmms_remote_pl_win_toggle(session, FALSE);
			return;
		}
	}
	g_print("%s: invalid parameter for playlist-show.\n",argv[0]);
	g_print("%s: syntax: %s playlist-show <on/off>\n",argv[0],argv[0]);
}

void equalizer_show(gint session, gint argc, gchar **argv)
{
	if (argc > 2)
	{
		if (!strncmp(argv[2],"on",2)) {
			xmms_remote_eq_win_toggle(session, TRUE);
			return;
		}
		else if (!strncmp(argv[2],"off",3)) {
			xmms_remote_eq_win_toggle(session, FALSE);
			return;
		}
	}
	g_print("%s: invalid parameter for equalizer-show.\n",argv[0]);
	g_print("%s: syntax: %s equalizer-show <on/off>\n",argv[0],argv[0]);
}

void show_preferences_window(gint session, gint argc, gchar **argv)
{
	xmms_remote_show_prefs_box(session);
}

void show_jtf_window(gint session, gint argc, gchar **argv)
{
	xmms_remote_show_jtf_box(session);
}

void shutdown_audacious_server(gint session, gint argc, gchar **argv)
{
	xmms_remote_quit(session);
}

void get_handlers_list(gint session, gint argc, gchar **argv)
{
	gint i;

	for (i = 0; handlers[i].name != NULL; i++)
	{
		if (!g_strcasecmp("<sep>", handlers[i].name))
			g_print("%s%s:\n", i == 0 ? "" : "\n", handlers[i].desc);
		else
			g_print("   %-34s - %s\n", handlers[i].name, handlers[i].desc);
	}

	g_print("\nHandlers may be prefixed with `--' (GNU-style long-options) or not, your choice.\n");
	g_print("Report bugs to http://bugs-meta.atheme.org/\n");
}