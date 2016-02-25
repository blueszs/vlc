/*****************************************************************************
 * file.c: File and crypt keystore
 *****************************************************************************
 * Copyright © 2015-2016 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <sys/stat.h>
#ifdef HAVE_FLOCK
#include <sys/file.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_memory.h>
#include <vlc_keystore.h>
#include <vlc_strings.h>

#include <assert.h>

#include "list_util.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("file keystore (plaintext)"))
    set_description(N_("secrets are stored on a file without any encryption"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_callbacks(Open, Close)
    add_string("keystore-file", NULL, NULL, NULL, true)
    set_capability("keystore", 0)
    add_shortcut("file_plaintext")
vlc_module_end ()

struct vlc_keystore_sys
{
    char *          psz_file;
};

static const char *const ppsz_keys[] = {
    "protocol",
    "user",
    "server",
    "path",
    "port",
    "realm",
    "authtype",
};
static_assert(sizeof(ppsz_keys)/sizeof(*ppsz_keys) == KEY_MAX, "key mismatch");

static int
str2key(const char *psz_key)
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (strcmp(ppsz_keys[i], psz_key) == 0)
            return i;
    }
    return -1;
}

static int
str_write(FILE *p_file, const char *psz_str)
{
    size_t i_len = strlen(psz_str);
    return fwrite(psz_str, sizeof(char), i_len, p_file) == i_len ? VLC_SUCCESS
                                                                 : VLC_EGENERIC;
}

static int
values_write(FILE *p_file, const char *const ppsz_values[KEY_MAX])
{
    for (unsigned int i = 0; i < KEY_MAX; ++i)
    {
        if (!ppsz_values[i])
            continue;
        if (str_write(p_file, ppsz_keys[i]))
            return VLC_EGENERIC;
        if (str_write(p_file, ":"))
            return VLC_EGENERIC;
        char *psz_b64 = vlc_b64_encode(ppsz_values[i]);
        if (!psz_b64 || str_write(p_file, psz_b64))
        {
            free(psz_b64);
            return VLC_EGENERIC;
        }
        free(psz_b64);
        for (unsigned int j = i + 1; j < KEY_MAX; ++j)
        {
            if (ppsz_values[j])
            {
                if (str_write(p_file, ","))
                    return VLC_EGENERIC;
                break;
            }
        }
    }

    return VLC_SUCCESS;
}

static int
truncate0(int i_fd)
{
#ifndef _WIN32
    return ftruncate(i_fd, 0) == 0 ? VLC_SUCCESS : VLC_EGENERIC;
#else
    return _chsize(i_fd, 0) == 0 ? VLC_SUCCESS : VLC_EGENERIC;
#endif
}

/* a line is "{key1:VALUE1_B64,key2:VALUE2_B64}:PASSWORD_B64" */
static int
file_save(vlc_keystore *p_keystore, FILE *p_file, int i_fd, struct ks_list *p_list)
{
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    int i_ret = VLC_EGENERIC;

    rewind(p_file);
    if (truncate0(i_fd))
    {
        vlc_unlink(p_sys->psz_file);
        return i_ret;
    }

    for (unsigned int i = 0; i < p_list->i_count; ++i)
    {
        vlc_keystore_entry *p_entry = &p_list->p_entries[i];
        if (!p_entry->p_secret)
            continue;

        if (str_write(p_file, "{"))
            goto end;
        if (values_write(p_file, (const char *const *) p_entry->ppsz_values))
            goto end;
        if (str_write(p_file, "}:"))
            goto end;
        char *psz_b64 = vlc_b64_encode_binary(p_entry->p_secret,
                                              p_entry->i_secret_len);
        if (!psz_b64 || str_write(p_file, psz_b64))
        {
            free(psz_b64);
            goto end;
        }
        free(psz_b64);
        if (i < p_list->i_count - 1 && str_write(p_file, "\n"))
            goto end;
    }
    i_ret = VLC_SUCCESS;
end:

    if (i_ret != VLC_SUCCESS)
    {
        if (truncate0(i_fd))
            vlc_unlink(p_sys->psz_file);
    }
    return i_ret;
}

static int
file_read(vlc_keystore *p_keystore, FILE *p_file, int i_fd, struct ks_list *p_list)
{
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    char *psz_line = NULL;
    size_t i_line_len = 0;
    ssize_t i_read;
    bool b_valid = false;

    while ((i_read = getline(&psz_line, &i_line_len, p_file)) != -1)
    {
        char *p = psz_line;
        if (*(p++) != '{')
        {
            getchar();
            goto end;
        }

        vlc_keystore_entry *p_entry = ks_list_new_entry(p_list);
        if (!p_entry)
            goto end;

        bool b_end = false;
        while (*p != '\0' && !b_end)
        {
            int i_key;
            char *p_key, *p_value;
            size_t i_len;

            /* read key */
            i_len = strcspn(p, ":");
            if (!i_len || p[i_len] == '\0')
                goto end;

            p[i_len] = '\0';
            p_key = p;
            i_key = str2key(p_key);
            if (i_key == -1 || i_key >= KEY_MAX)
                goto end;
            p += i_len + 1;

            /* read value */
            i_len = strcspn(p, ",}");
            if (!i_len || p[i_len] == '\0')
                goto end;

            if (p[i_len] == '}')
                b_end = true;

            p[i_len] = '\0';
            p_value = vlc_b64_decode(p); /* BASE 64 */
            if (!p_value)
                goto end;
            p += i_len + 1;

            p_entry->ppsz_values[i_key] = p_value;
        }
        /* read passwd */
        if (*p == '\0' || *p != ':')
            goto end;

        p_entry->i_secret_len = vlc_b64_decode_binary(&p_entry->p_secret, p + 1);
        if (!p_entry->p_secret)
            goto end;
    }

    b_valid = true;

end:
    free(psz_line);
    if (!b_valid)
    {
        if (truncate0(i_fd))
            vlc_unlink(p_sys->psz_file);
    }
    return VLC_SUCCESS;
}

static int
file_open(const char *psz_file, const char *psz_mode, FILE **pp_file)
{
    FILE *p_file = vlc_fopen(psz_file, psz_mode);
    if (p_file == NULL)
        return -1;

    int i_fd = fileno(p_file);
    if (i_fd == -1)
        return -1;

#ifdef HAVE_FLOCK
    if (flock(i_fd, LOCK_EX) != 0)
    {
        fclose(p_file);
    }
#endif
    *pp_file = p_file;
    return i_fd;
}

static void
file_close(FILE *p_file, int i_fd)
{
#ifdef HAVE_FLOCK
    flock(i_fd, LOCK_UN);
#endif
    fclose(p_file);
}

static int
Store(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
      const uint8_t *p_secret, size_t i_secret_len, const char *psz_label)
{
    (void) psz_label;
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    int i_ret = VLC_EGENERIC;
    struct ks_list list = { 0 };
    FILE *p_file;
    int i_fd = file_open(p_sys->psz_file, "r+", &p_file);
    if (i_fd == -1)
        return i_ret;

    if (file_read(p_keystore, p_file, i_fd, &list) != VLC_SUCCESS)
        goto end;

    vlc_keystore_entry *p_entry = ks_list_find_entry(&list, ppsz_values, NULL);

    if (p_entry)
        vlc_keystore_release_entry(p_entry);
    else
    {
        p_entry = ks_list_new_entry(&list);
        if (!p_entry)
            goto end;
    }
    if (ks_values_copy((const char **)p_entry->ppsz_values, ppsz_values))
        goto end;

    if (vlc_keystore_entry_set_secret(p_entry, p_secret, i_secret_len))
        goto end;

    i_ret = file_save(p_keystore, p_file, i_fd, &list);

end:
    file_close(p_file, i_fd);
    ks_list_free(&list);
    return i_ret;
}

static unsigned int
Find(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX],
     vlc_keystore_entry **pp_entries)
{
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    struct ks_list list = { 0 };
    struct ks_list out_list = { 0 };
    FILE *p_file;
    int i_fd = file_open(p_sys->psz_file, "r", &p_file);
    if (i_fd == -1)
        return 0;

    if (file_read(p_keystore, p_file, i_fd, &list) != VLC_SUCCESS)
        goto end;

    vlc_keystore_entry *p_entry;
    unsigned i_index = 0;
    while ((p_entry = ks_list_find_entry(&list, ppsz_values, &i_index)))
    {
        vlc_keystore_entry *p_out_entry = ks_list_new_entry(&out_list);

        if (!p_out_entry
         || ks_values_copy((const char **)p_out_entry->ppsz_values,
                           (const char *const*)p_entry->ppsz_values))
        {
            ks_list_free(&out_list);
            goto end;
        }

        if (vlc_keystore_entry_set_secret(p_out_entry, p_entry->p_secret,
                                          p_entry->i_secret_len))
        {
            ks_list_free(&out_list);
            goto end;
        }
    }

    *pp_entries = out_list.p_entries;
end:
    file_close(p_file, i_fd);
    ks_list_free(&list);
    return out_list.i_count;
}

static unsigned int
Remove(vlc_keystore *p_keystore, const char *const ppsz_values[KEY_MAX])
{
    vlc_keystore_sys *p_sys = p_keystore->p_sys;
    unsigned int i_count = 0;
    struct ks_list list = { 0 };
    FILE *p_file;
    int i_fd = file_open(p_sys->psz_file, "r+", &p_file);
    if (i_fd == -1)
        return 0;

    if (file_read(p_keystore, p_file, i_fd, &list) != VLC_SUCCESS)
        goto end;

    vlc_keystore_entry *p_entry;
    unsigned i_index = 0;
    while ((p_entry = ks_list_find_entry(&list, ppsz_values, &i_index)))
    {
        vlc_keystore_release_entry(p_entry);
        i_count++;
    }

    if (i_count > 0
     && file_save(p_keystore, p_file, i_fd, &list) != VLC_SUCCESS)
        i_count = 0;

end:
    file_close(p_file, i_fd);
    ks_list_free(&list);
    return i_count;
}

static void
Close(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;
    vlc_keystore_sys *p_sys = p_keystore->p_sys;

    free(p_sys->psz_file);
    free(p_sys);
}

static int
Open(vlc_object_t *p_this)
{
    vlc_keystore *p_keystore = (vlc_keystore *)p_this;

    vlc_keystore_sys *p_sys = calloc(1, sizeof(vlc_keystore_sys));
    if (!p_sys)
        return VLC_EGENERIC;

    char *psz_file = var_InheritString(p_this, "keystore-file");
    if (!psz_file)
    {
        free(p_sys);
        return VLC_EGENERIC;
    }

    struct stat stat;
    bool b_file_exists = false;
    if (vlc_stat(psz_file, &stat) != 0)
    {
        FILE *p_file = vlc_fopen(psz_file, "a+");
        if (p_file != NULL)
        {
            b_file_exists= true;
            fclose(p_file);
        }
    }
    else
        b_file_exists = true;

    if (!b_file_exists)
    {
        free(p_sys);
        free(psz_file);
        return VLC_EGENERIC;
    }

    p_sys->psz_file = psz_file;
    p_keystore->p_sys = p_sys;
    p_keystore->pf_store = Store;
    p_keystore->pf_find = Find;
    p_keystore->pf_remove = Remove;

    return VLC_SUCCESS;
}
