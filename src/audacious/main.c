/*
 * main.c
 * Copyright 2007-2011 William Pitcock and John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

#include <gtk/gtk.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>
#include <libaudtag/audtag.h>

#ifdef USE_DBUS
#include "../libaudclient/audctrl.h"
#include "dbus.h"
#endif

#include "debug.h"
#include "drct.h"
#include "equalizer.h"
#include "i18n.h"
#include "interface.h"
#include "main.h"
#include "misc.h"
#include "playlist.h"
#include "plugins.h"
#include "scanner.h"
#include "util.h"

#define AUTOSAVE_INTERVAL 300 /* seconds */

bool_t headless;

static struct {
    char **filenames;
    int session;
    bool_t play, stop, pause, fwd, rew, play_pause, show_jump_box;
    bool_t enqueue, mainwin, remote;
    bool_t enqueue_to_temp;
    bool_t quit_after_play;
    bool_t version;
    bool_t verbose;
    char *previous_session_id;
} options;

static char * aud_paths[AUD_PATH_COUNT];

static void make_dirs(void)
{
#ifdef S_IRGRP
    const mode_t mode755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
#else
    const mode_t mode755 = S_IRWXU;
#endif

    make_directory(aud_paths[AUD_PATH_USER_DIR], mode755);
    make_directory(aud_paths[AUD_PATH_PLAYLISTS_DIR], mode755);
}

static void normalize_path (char * path)
{
#ifdef _WIN32
    string_replace_char (path, '/', '\\');
#endif
    int len = strlen (path);
#ifdef _WIN32
    if (len > 3 && path[len - 1] == '\\') /* leave "C:\" */
#else
    if (len > 1 && path[len - 1] == '/') /* leave leading "/" */
#endif
        path[len - 1] = 0;
}

static char * last_path_element (char * path)
{
    char * slash = strrchr (path, G_DIR_SEPARATOR);
    return (slash && slash[1]) ? slash + 1 : NULL;
}

static void strip_path_element (char * path, char * elem)
{
#ifdef _WIN32
    if (elem > path + 3)
#else
    if (elem > path + 1)
#endif
        elem[-1] = 0; /* overwrite slash */
    else
        elem[0] = 0; /* leave [drive letter and] leading slash */
}

static void relocate_path (char * * pathp, const char * old, const char * new)
{
    char * path = * pathp;
    int oldlen = strlen (old);
    int newlen = strlen (new);

    if (oldlen && old[oldlen - 1] == G_DIR_SEPARATOR)
        oldlen --;
    if (newlen && new[newlen - 1] == G_DIR_SEPARATOR)
        newlen --;

#ifdef _WIN32
    if (strncasecmp (path, old, oldlen) || (path[oldlen] && path[oldlen] != G_DIR_SEPARATOR))
#else
    if (strncmp (path, old, oldlen) || (path[oldlen] && path[oldlen] != G_DIR_SEPARATOR))
#endif
    {
        fprintf (stderr, "Failed to relocate a data path.  Falling back to "
         "compile-time path: %s\n", path);
        return;
    }

    * pathp = g_strdup_printf ("%.*s%s", newlen, new, path + oldlen);
    g_free (path);
}

static void relocate_paths (void)
{
    /* Start with the paths hard coded at compile time. */
    aud_paths[AUD_PATH_BIN_DIR] = g_strdup (HARDCODE_BINDIR);
    aud_paths[AUD_PATH_DATA_DIR] = g_strdup (HARDCODE_DATADIR);
    aud_paths[AUD_PATH_PLUGIN_DIR] = g_strdup (HARDCODE_PLUGINDIR);
    aud_paths[AUD_PATH_LOCALE_DIR] = g_strdup (HARDCODE_LOCALEDIR);
    aud_paths[AUD_PATH_DESKTOP_FILE] = g_strdup (HARDCODE_DESKTOPFILE);
    aud_paths[AUD_PATH_ICON_FILE] = g_strdup (HARDCODE_ICONFILE);
    normalize_path (aud_paths[AUD_PATH_BIN_DIR]);
    normalize_path (aud_paths[AUD_PATH_DATA_DIR]);
    normalize_path (aud_paths[AUD_PATH_PLUGIN_DIR]);
    normalize_path (aud_paths[AUD_PATH_LOCALE_DIR]);
    normalize_path (aud_paths[AUD_PATH_DESKTOP_FILE]);
    normalize_path (aud_paths[AUD_PATH_ICON_FILE]);

    /* Compare the compile-time path to the executable and the actual path to
     * see if we have been moved. */
    char * old = g_strdup (aud_paths[AUD_PATH_BIN_DIR]);
    char * new = get_path_to_self ();
    if (! new)
    {
ERR:
        g_free (old);
        g_free (new);
        return;
    }
    normalize_path (new);

    /* Strip the name of the executable file, leaving the path. */
    char * base = last_path_element (new);
    if (! base)
        goto ERR;
    strip_path_element (new, base);

    /* Strip innermost folder names from both paths as long as they match.  This
     * leaves a compile-time prefix and a run-time one to replace it with. */
    char * a, * b;
    while ((a = last_path_element (old)) && (b = last_path_element (new)) &&
#ifdef _WIN32
     ! strcasecmp (a, b))
#else
     ! strcmp (a, b))
#endif
    {
        strip_path_element (old, a);
        strip_path_element (new, b);
    }

    /* Do the replacements. */
    relocate_path (& aud_paths[AUD_PATH_BIN_DIR], old, new);
    relocate_path (& aud_paths[AUD_PATH_DATA_DIR], old, new);
    relocate_path (& aud_paths[AUD_PATH_PLUGIN_DIR], old, new);
    relocate_path (& aud_paths[AUD_PATH_LOCALE_DIR], old, new);
    relocate_path (& aud_paths[AUD_PATH_DESKTOP_FILE], old, new);
    relocate_path (& aud_paths[AUD_PATH_ICON_FILE], old, new);

    g_free (old);
    g_free (new);
}

static void init_paths (void)
{
    relocate_paths ();

    const char * xdg_config_home = g_get_user_config_dir ();
    const char * xdg_data_home = g_get_user_data_dir ();

#ifdef _WIN32
    /* Some libraries (libmcs) and plugins (filewriter) use these variables,
     * which are generally not set on Windows. */
    g_setenv ("HOME", g_get_home_dir (), TRUE);
    g_setenv ("XDG_CONFIG_HOME", xdg_config_home, TRUE);
    g_setenv ("XDG_DATA_HOME", xdg_data_home, TRUE);
    g_setenv ("XDG_CACHE_HOME", g_get_user_cache_dir (), TRUE);
#endif

    aud_paths[AUD_PATH_USER_DIR] = g_build_filename(xdg_config_home, "audacious", NULL);
    aud_paths[AUD_PATH_USER_PLUGIN_DIR] = g_build_filename(xdg_data_home, "audacious", "Plugins", NULL);
    aud_paths[AUD_PATH_PLAYLISTS_DIR] = g_build_filename(aud_paths[AUD_PATH_USER_DIR], "playlists", NULL);
    aud_paths[AUD_PATH_GTKRC_FILE] = g_build_filename(aud_paths[AUD_PATH_USER_DIR], "gtkrc", NULL);

    for (int i = 0; i < AUD_PATH_COUNT; i ++)
        AUDDBG ("Data path: %s\n", aud_paths[i]);
}

const char * get_path (int id)
{
    g_return_val_if_fail (id >= 0 && id < AUD_PATH_COUNT, NULL);
    return aud_paths[id];
}

static GOptionEntry cmd_entries[] = {
    {"rew", 'r', 0, G_OPTION_ARG_NONE, &options.rew, N_("Skip backwards in playlist"), NULL},
    {"play", 'p', 0, G_OPTION_ARG_NONE, &options.play, N_("Start playing current playlist"), NULL},
    {"pause", 'u', 0, G_OPTION_ARG_NONE, &options.pause, N_("Pause current song"), NULL},
    {"stop", 's', 0, G_OPTION_ARG_NONE, &options.stop, N_("Stop current song"), NULL},
    {"play-pause", 't', 0, G_OPTION_ARG_NONE, &options.play_pause, N_("Pause if playing, play otherwise"), NULL},
    {"fwd", 'f', 0, G_OPTION_ARG_NONE, &options.fwd, N_("Skip forward in playlist"), NULL},
    {"show-jump-box", 'j', 0, G_OPTION_ARG_NONE, &options.show_jump_box, N_("Display Jump to File dialog"), NULL},
    {"enqueue", 'e', 0, G_OPTION_ARG_NONE, &options.enqueue, N_("Add files to the playlist"), NULL},
    {"enqueue-to-temp", 'E', 0, G_OPTION_ARG_NONE, &options.enqueue_to_temp, N_("Add new files to a temporary playlist"), NULL},
    {"show-main-window", 'm', 0, G_OPTION_ARG_NONE, &options.mainwin, N_("Display the main window"), NULL},
    {"headless", 'h', 0, G_OPTION_ARG_NONE, & headless, N_("Headless mode"), NULL},
    {"quit-after-play", 'q', 0, G_OPTION_ARG_NONE, &options.quit_after_play, N_("Quit on playback stop"), NULL},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &options.version, N_("Show version"), NULL},
    {"verbose", 'V', 0, G_OPTION_ARG_NONE, &options.verbose, N_("Print debugging messages"), NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &options.filenames, N_("FILE..."), NULL},
    {NULL},
};

static void parse_options (int * argc, char *** argv)
{
    GOptionContext *context;
    GError *error = NULL;

    memset (& options, 0, sizeof options);
    options.session = -1;

    context = g_option_context_new(_("- play multimedia files"));
    g_option_context_add_main_entries(context, cmd_entries, PACKAGE);
    g_option_context_add_group(context, gtk_get_option_group(FALSE));

    if (!g_option_context_parse(context, argc, argv, &error))
    {
        fprintf (stderr,
         _("%s: %s\nTry `%s --help' for more information.\n"), (* argv)[0],
         error->message, (* argv)[0]);
        exit (EXIT_FAILURE);
    }

    g_option_context_free (context);

    verbose = options.verbose;
}

static bool_t get_lock (void)
{
    char * path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "lock", aud_paths[AUD_PATH_USER_DIR]);
    int handle = open (path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

    if (handle < 0)
    {
        if (errno != EEXIST)
            fprintf (stderr, "Cannot create %s: %s.\n", path, strerror (errno));

        g_free (path);
        return FALSE;
    }

    close (handle);
    g_free (path);
    return TRUE;
}

static void release_lock (void)
{
    char * path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "lock", aud_paths[AUD_PATH_USER_DIR]);
    unlink (path);
    g_free (path);
}

static Index * convert_filenames (void)
{
    if (! options.filenames)
        return NULL;

    Index * filenames = index_new ();
    char * * f = options.filenames;
    char * cur = g_get_current_dir ();

    for (int i = 0; f[i]; i ++)
    {
        char * uri = NULL;

        if (strstr (f[i], "://"))
            uri = str_get (f[i]);
        else if (g_path_is_absolute (f[i]))
        {
            char * tmp = filename_to_uri (f[i]);
            uri = str_get (tmp);
            free (tmp);
        }
        else
        {
            char * tmp = g_build_filename (cur, f[i], NULL);
            char * tmp2 = filename_to_uri (tmp);
            uri = str_get (tmp2);
            free (tmp);
            free (tmp2);
        }

        if (uri)
            index_append (filenames, uri);
    }

    g_free (cur);
    return filenames;
}

static void do_remote (void)
{
#ifdef USE_DBUS
    DBusGProxy * session = audacious_get_dbus_proxy ();

    if (session && audacious_remote_is_running (session))
    {
        Index * filenames = convert_filenames ();

        /* if no command line options, then present running instance */
        if (! (filenames || options.play || options.pause || options.play_pause ||
         options.stop || options.rew || options.fwd || options.show_jump_box ||
         options.mainwin))
            options.mainwin = TRUE;

        if (filenames)
        {
            GList * list = NULL;

            for (int f = index_count (filenames); f --; )
                list = g_list_prepend (list, index_get (filenames, f));

            if (options.enqueue_to_temp)
                audacious_remote_playlist_open_list_to_temp (session, list);
            else if (options.enqueue)
                audacious_remote_playlist_add (session, list);
            else
                audacious_remote_playlist_open_list (session, list);

            g_list_free (list);

            for (int f = 0; f < index_count (filenames); f ++)
                str_unref (index_get (filenames, f));

            index_free (filenames);
        }

        if (options.play)
            audacious_remote_play (session);
        if (options.pause)
            audacious_remote_pause (session);
        if (options.play_pause)
            audacious_remote_play_pause (session);
        if (options.stop)
            audacious_remote_stop (session);
        if (options.rew)
            audacious_remote_playlist_prev (session);
        if (options.fwd)
            audacious_remote_playlist_next (session);
        if (options.show_jump_box)
            audacious_remote_show_jtf_box (session);
        if (options.mainwin)
            audacious_remote_main_win_toggle (session, TRUE);

        exit (EXIT_SUCCESS);
    }
#endif

    fprintf (stderr, "WARNING: Audacious seems to be already running but is not responding.\n");
}

static void do_commands (void)
{
    bool_t resume = get_bool (NULL, "resume_playback_on_startup");

    Index * filenames = convert_filenames ();
    if (filenames)
    {
        if (options.enqueue_to_temp)
        {
            drct_pl_open_temp_list (filenames);
            resume = FALSE;
        }
        else if (options.enqueue)
            drct_pl_add_list (filenames, -1);
        else
        {
            drct_pl_open_list (filenames);
            resume = FALSE;
        }
    }

    if (resume)
        playlist_resume ();

    if (options.play || options.play_pause)
    {
        if (! drct_get_playing ())
            drct_play ();
        else if (drct_get_paused ())
            drct_pause ();
    }

    if (options.show_jump_box)
        interface_show_jump_to_track ();
    if (options.mainwin)
        interface_show (TRUE);
}

static void init_one (void)
{
    init_paths ();
    make_dirs ();

    setlocale (LC_ALL, "");
    bindtextdomain (PACKAGE, aud_paths[AUD_PATH_LOCALE_DIR]);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    bindtextdomain (PACKAGE "-plugins", aud_paths[AUD_PATH_LOCALE_DIR]);
    bind_textdomain_codeset (PACKAGE "-plugins", "UTF-8");
    textdomain (PACKAGE);
}

static void init_two (int * p_argc, char * * * p_argv)
{
    if (! headless)
    {
#if ! GLIB_CHECK_VERSION (2, 32, 0)
        g_thread_init (NULL);
#endif
        gtk_init (p_argc, p_argv);
    }

    AUDDBG ("Loading configuration.\n");
    config_load ();

    AUDDBG ("Initializing.\n");
    art_init ();
    chardet_init ();
    eq_init ();
    playlist_init ();

#ifdef HAVE_SIGWAIT
    signals_init ();
#endif

    tag_set_verbose (verbose);
    vfs_set_verbose (verbose);

    AUDDBG ("Loading lowlevel plugins.\n");
    start_plugins_one ();

    AUDDBG ("Starting worker threads.\n");
    adder_init ();
    scanner_init ();

    AUDDBG ("Restoring state.\n");
    load_playlists ();

    do_commands ();

    AUDDBG ("Loading highlevel plugins.\n");
    start_plugins_two ();

#ifdef USE_DBUS
    init_dbus ();
#endif

    mpris_signals_init ();
}

static void shut_down (void)
{
    mpris_signals_cleanup ();

    AUDDBG ("Saving playlist state.\n");
    save_playlists (TRUE);

    AUDDBG ("Unloading highlevel plugins.\n");
    stop_plugins_two ();

#ifdef USE_DBUS
    cleanup_dbus ();
#endif

    AUDDBG ("Stopping playback.\n");
    if (drct_get_playing ())
        drct_stop ();

    AUDDBG ("Stopping worker threads.\n");
    adder_cleanup ();
    scanner_cleanup ();

    AUDDBG ("Unloading lowlevel plugins.\n");
    stop_plugins_one ();

    AUDDBG ("Saving configuration.\n");
    config_save ();
    config_cleanup ();

    AUDDBG ("Cleaning up.\n");
    art_cleanup ();
    eq_cleanup ();
    history_cleanup ();
    playlist_end ();

    strpool_shutdown ();
}

bool_t do_autosave (void)
{
    AUDDBG ("Saving configuration.\n");
    hook_call ("config save", NULL);
    save_playlists (FALSE);
    config_save ();
    return TRUE;
}

static bool_t check_should_quit (void)
{
    return options.quit_after_play && ! drct_get_playing () && ! playlist_add_in_progress (-1);
}

static void maybe_quit (void)
{
    if (check_should_quit ())
        gtk_main_quit ();
}

int main(int argc, char ** argv)
{
    init_one ();
    parse_options (& argc, & argv);

    if (options.version)
    {
        printf ("%s %s (%s)\n", _("Audacious"), VERSION, BUILDSTAMP);
        return EXIT_SUCCESS;
    }

    if (! get_lock ())
        do_remote (); /* may exit */

    AUDDBG ("No remote session; starting up.\n");
    init_two (& argc, & argv);

    AUDDBG ("Startup complete.\n");
    g_timeout_add_seconds (AUTOSAVE_INTERVAL, (GSourceFunc) do_autosave, NULL);

    if (check_should_quit ())
        goto QUIT;

    hook_associate ("playback stop", (HookFunction) maybe_quit, NULL);
    hook_associate ("playlist add complete", (HookFunction) maybe_quit, NULL);
    hook_associate ("quit", (HookFunction) gtk_main_quit, NULL);

    gtk_main ();

    hook_dissociate ("playback stop", (HookFunction) maybe_quit);
    hook_dissociate ("playlist add complete", (HookFunction) maybe_quit);
    hook_dissociate ("quit", (HookFunction) gtk_main_quit);

QUIT:
    shut_down ();
    release_lock ();
    return EXIT_SUCCESS;
}
