/*  Audacious - Cross-platform multimedia player
 *  Copyright (C) 2005-2007  Audacious development team
 *
 *  Based on BMP:
 *  Copyright (C) 2003-2004  BMP development team.
 *
 *  Based on XMMS:
 *  Copyright (C) 1998-2003  XMMS development team.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 2 of the License.
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "libaudacious/util.h"
#include "libaudacious/configdb.h"

#include "input.h"
#include "main.h"
#include "ui_main.h"
#include "ui_equalizer.h"
#include "output.h"
#include "playlist.h"
#include "ui_playlist.h"
#include "ui_skinselector.h"
#include "urldecode.h"
#include "util.h"

#include "playback.h"

#include "hook.h"

/* FIXME: yuck!! this shouldn't be here... */
void
playback_set_random_skin(void)
{
    SkinNode *node;
    guint32 randval;

    /* Get a random value to select the skin to use */
    randval = g_random_int_range(0, g_list_length(skinlist));
    node = g_list_nth(skinlist, randval)->data;
    bmp_active_skin_load(node->path);
}

gint
playback_get_time(void)
{
    if (!playback_get_playing())
        return -1;

    if (!get_current_input_plugin())
        return -1;

    return get_current_input_plugin()->get_time();
}

void
playback_initiate(void)
{
    PlaylistEntry *entry = NULL;
    Playlist *playlist = playlist_get_active();
    gint penalty = 0;

    if (playlist_get_length(playlist) == 0)
        return;

    if (playback_get_playing())
        playback_stop();

    vis_clear_data(mainwin_vis);
    svis_clear_data(mainwin_svis);
    mainwin_disable_seekbar();

    entry = playlist_get_entry_to_play(playlist);

    if (entry == NULL)
        return;

    /*
     * If the playlist entry cannot be played, try to pick another one.
     * If that does not work, e.g. entry == NULL, then bail.
     *
     *   - nenolod
     */
    for ( penalty=0 ; penalty <= 15 && entry != NULL && !playback_play_file(entry) ; penalty++ )
    {
        playlist_next(playlist);

        entry = playlist_get_entry_to_play(playlist);

	/* XXX ew. workaround for a stupid bug where audacious will keep 
	 * trying to play a file with no valid decoder.
	 */
        if (entry == NULL)
            return;

	if (entry->decoder == NULL )
	    entry->decoder = input_check_file(entry->filename, FALSE);

	/* if we hit 15 entries in a row with no valid decoder, just 
         * bail due to confusion
	 */
	if (penalty == 15)
	    return;
    }

    if (playback_get_time() != -1) {
        equalizerwin_load_auto_preset(entry->filename);
        input_set_eq(cfg.equalizer_active, cfg.equalizer_preamp,
                     cfg.equalizer_bands);
        output_set_eq(cfg.equalizer_active, cfg.equalizer_preamp,
                      cfg.equalizer_bands);
    }

    playlist_check_pos_current(playlist);
    mainwin_set_info_text();

    hook_call("playback begin", entry);
}

void
playback_pause(void)
{
    if (!playback_get_playing())
        return;

    if (!get_current_input_plugin())
        return;

    ip_data.paused = !ip_data.paused;

    if (get_current_input_plugin()->pause)
        get_current_input_plugin()->pause(ip_data.paused);

    g_return_if_fail(mainwin_playstatus != NULL);

    if (ip_data.paused)
        playstatus_set_status(mainwin_playstatus, STATUS_PAUSE);
    else
        playstatus_set_status(mainwin_playstatus, STATUS_PLAY);
}

void
playback_stop(void)
{
    if (ip_data.playing && get_current_input_plugin()) {

        if (playback_get_paused()) {
            output_flush(get_written_time()); /* to avoid noise */
            playback_pause();
        }

        ip_data.playing = FALSE; 

        if (get_current_input_plugin()->stop)
            get_current_input_plugin()->stop();

        free_vis_data();
        ip_data.paused = FALSE;

        if (input_info_text) {
            g_free(input_info_text);
            input_info_text = NULL;
            mainwin_set_info_text();
        }
    }

    ip_data.buffering = FALSE;
    ip_data.playing = FALSE;
    
    g_return_if_fail(mainwin_playstatus != NULL);
    playstatus_set_status_buffering(mainwin_playstatus, FALSE);
}

static void
run_no_output_plugin_dialog(void)
{
    const gchar *markup = 
        N_("<b><big>No output plugin selected.</big></b>\n"
           "You have not selected an output plugin.");

    GtkWidget *dialog =
        gtk_message_dialog_new_with_markup(GTK_WINDOW(mainwin),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           _(markup));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

gboolean
playback_play_file(PlaylistEntry *entry)
{
    g_return_val_if_fail(entry != NULL, FALSE);

    if (!get_current_output_plugin()) {
        run_no_output_plugin_dialog();
        mainwin_stop_pushed();
        return FALSE;
    }

    if (cfg.random_skin_on_play)
        playback_set_random_skin();

    /*
     * This is slightly uglier than the original version, but should
     * fix the "crash" issues as seen in 0.2 when dealing with this situation.
     *  - nenolod
     */
    if (!entry->decoder && 
	(((entry->decoder = input_check_file(entry->filename, FALSE)) == NULL) ||
        !input_is_enabled(entry->decoder->filename)))
    {
        input_file_not_playable(entry->filename);

        set_current_input_plugin(NULL);
        mainwin_set_info_text();

        return FALSE;
    }

    set_current_input_plugin(entry->decoder);
    entry->decoder->output = &psuedo_output_plugin;
    entry->decoder->play_file(entry->filename);

    ip_data.playing = TRUE;

    return TRUE;
}

gboolean
playback_get_playing(void)
{
    return ip_data.playing;
}

gboolean
playback_get_paused(void)
{
    return ip_data.paused;
}

void
playback_seek(gint time)
{
    gboolean restore_pause = FALSE;
    gint l=0, r=0;

    if (!ip_data.playing)
        return;

    if (!get_current_input_plugin())
        return;

    /* FIXME WORKAROUND...that should work with all plugins
     * mute the volume, start playback again, do the seek, then pause again
     * -Patrick Sudowe 
     */
    if(ip_data.paused)
    {
	restore_pause = TRUE;
	output_get_volume(&l, &r);
	output_set_volume(0,0);
	playback_pause();	
    }
    
    free_vis_data();
    get_current_input_plugin()->seek(time);
    
    if(restore_pause)
    {
	playback_pause();
	output_set_volume(l, r);
    }
}

void
playback_seek_relative(gint offset)
{
    gint time = CLAMP(playback_get_time() / 1000 + offset,
                      0, playlist_get_current_length(playlist_get_active()) / 1000 - 1);
    playback_seek(time);
}
