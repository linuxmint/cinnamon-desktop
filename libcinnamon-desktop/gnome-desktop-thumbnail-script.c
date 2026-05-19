/*
 * Copyright (C) 2002, 2017 Red Hat, Inc.
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * This file is part of the Cinnamon Desktop Library.
 *
 * The Cinnamon Desktop Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * The Cinnamon Desktop Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Cinnamon Desktop Library; see the file COPYING.LIB.
 * If not, write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Originally derived from libgnome-desktop's gnome-desktop-thumbnail-script.c
 * (LGPL-2+), simplified for Cinnamon: no Flatpak/Snap detection and no
 * flatpak-spawn path.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Carlos Garcia Campos <carlosgc@gnome.org>
 *          Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef ENABLE_SECCOMP
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/personality.h>
#include <sched.h>
#include <seccomp.h>
#endif

#include "gnome-desktop-thumbnail-script.h"

#define GST_REGISTRY_FILENAME "gstreamer-1.0.registry"

typedef struct
{
  gboolean use_sandbox;
  gboolean has_gst_registry;
  char *thumbnailer_name;
  GArray *fd_array;
  /* Input/output file paths outside the sandbox */
  char *infile;
  char *outfile;
  char *outdir; /* parent dir of outfile when sandboxed; needs rmdir on free */
  char *infile_tmp; /* host-side path of the bwrap mount-point stub for s_infile;
                     * bwrap creates this empty file inside outdir to mount infile
                     * over, and it persists after bwrap exits. */
  /* I/O file paths as visible inside the sandbox (NULL when unsandboxed) */
  char *s_infile;
  char *s_outfile;
} ScriptExec;

static char *
create_gst_cache_dir (void)
{
  char *out;

  out = g_build_filename (g_get_user_cache_dir (),
                          "cinnamon-desktop-thumbnailer",
                          "gstreamer-1.0",
                          NULL);
  if (g_mkdir_with_parents (out, 0700) < 0)
    g_clear_pointer (&out, g_free);
  return out;
}

static char *
expand_thumbnailing_elem (const char *elem,
                          const int   size,
                          const char *mime_type,
                          const char *infile,
                          const char *outfile,
                          gboolean   *got_input,
                          gboolean   *got_output)
{
  GString *str;
  const char *p, *last;
  char *inuri;

  str = g_string_new (NULL);

  last = elem;
  while ((p = strchr (last, '%')) != NULL)
    {
      g_string_append_len (str, last, p - last);
      p++;

      switch (*p)
        {
        case 'u':
          inuri = g_filename_to_uri (infile, NULL, NULL);
          if (inuri)
            {
              g_string_append (str, inuri);
              *got_input = TRUE;
              g_free (inuri);
            }
          p++;
          break;
        case 'i':
          g_string_append (str, infile);
          *got_input = TRUE;
          p++;
          break;
        case 'o':
          g_string_append (str, outfile);
          *got_output = TRUE;
          p++;
          break;
        case 's':
          g_string_append_printf (str, "%d", size);
          p++;
          break;
        case 'm':
          g_string_append (str, mime_type);
          p++;
          break;
        case '%':
          g_string_append_c (str, '%');
          p++;
          break;
        case 0:
        default:
          break;
        }
      last = p;
    }
  g_string_append (str, last);

  return g_string_free (str, FALSE);
}

G_GNUC_NULL_TERMINATED
static void
add_args (GPtrArray *array, ...)
{
  va_list args;
  const gchar *arg;

  va_start (args, array);
  while ((arg = va_arg (args, const gchar *)) != NULL)
    g_ptr_array_add (array, g_strdup (arg));
  va_end (args);
}

#ifdef ENABLE_SECCOMP

G_GNUC_PRINTF (2, 3)
static gboolean
seccomp_fail (GError **error, const char *fmt, ...)
{
  va_list ap;
  char *msg;

  va_start (ap, fmt);
  msg = g_strdup_vprintf (fmt, ap);
  va_end (ap);

  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, msg);
  g_free (msg);
  return FALSE;
}

/* libseccomp mostly returns negative errno values, but some are used for
 * non-standard purposes where strerror() would mislead. */
static const char *
seccomp_strerror (int negative_errno)
{
  g_return_val_if_fail (negative_errno < 0, "Non-negative error value from libseccomp?");
  g_return_val_if_fail (negative_errno > INT_MIN, "Out of range error value from libseccomp?");

  switch (negative_errno)
    {
    case -EDOM:
      return "Architecture specific failure";
    case -EFAULT:
      return "Internal libseccomp failure (unknown syscall?)";
    case -ECANCELED:
      return "System failure beyond the control of libseccomp";
    }

  return g_strerror (-negative_errno);
}

/* 32-bit compat sibling so the sandbox also covers multiarch thumbnailers
 * (e.g. an i386 binary running on amd64). */
#if defined(__x86_64__)
static const guint32 seccomp_extra_arches[] = { SCMP_ARCH_X86, 0 };
#elif defined(__aarch64__)
static const guint32 seccomp_extra_arches[] = { SCMP_ARCH_ARM, 0 };
#else
#error "Unsupported architecture for seccomp sandbox; build with -Dbubblewrap=disabled"
#endif

static inline void
cleanup_seccomp (void *p)
{
  scmp_filter_ctx *pp = (scmp_filter_ctx *) p;
  if (*pp)
    seccomp_release (*pp);
}

static gboolean
setup_seccomp (GPtrArray  *argv_array,
               GArray     *fd_array,
               GError    **error)
{
  __attribute__((cleanup (cleanup_seccomp))) scmp_filter_ctx seccomp = NULL;
#if defined(__x86_64__)
  const guint32 arch_id = SCMP_ARCH_X86_64;
#elif defined(__aarch64__)
  const guint32 arch_id = SCMP_ARCH_AARCH64;
#endif

  /* Syscall blocklist sourced from flatpak's common/flatpak-run.c
   * (setup_seccomp). Refresh when picking up upstream fixes. */
  struct
  {
    int                  scall;
    int                  errnum;
    struct scmp_arg_cmp *arg;
  } syscall_blocklist[] = {
    /* Block dmesg */
    {SCMP_SYS (syslog), EPERM},
    /* Useless old syscall */
    {SCMP_SYS (uselib), EPERM},
    /* Don't allow disabling accounting */
    {SCMP_SYS (acct), EPERM},
    /* 16-bit code is unnecessary in the sandbox, and modify_ldt is a
       historic source of interesting information leaks. */
    {SCMP_SYS (modify_ldt), EPERM},
    /* Don't allow reading current quota use */
    {SCMP_SYS (quotactl), EPERM},

    /* Kernel keyring */
    {SCMP_SYS (add_key), EPERM},
    {SCMP_SYS (keyctl), EPERM},
    {SCMP_SYS (request_key), EPERM},

    /* Scary VM/NUMA ops */
    {SCMP_SYS (move_pages), EPERM},
    {SCMP_SYS (mbind), EPERM},
    {SCMP_SYS (get_mempolicy), EPERM},
    {SCMP_SYS (set_mempolicy), EPERM},
    {SCMP_SYS (migrate_pages), EPERM},

    /* No subnamespace setups: */
    {SCMP_SYS (unshare), EPERM},
    {SCMP_SYS (setns), EPERM},
    {SCMP_SYS (mount), EPERM},
    {SCMP_SYS (umount), EPERM},
    {SCMP_SYS (umount2), EPERM},
    {SCMP_SYS (pivot_root), EPERM},
    {SCMP_SYS (chroot), EPERM},
    {SCMP_SYS (clone), EPERM, &SCMP_A0 (SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER)},

    /* CVE-2017-5226: don't allow faking input to the controlling tty */
    {SCMP_SYS (ioctl), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, (int) TIOCSTI)},
    /* CVE-2023-28100: copy/paste on a Linux virtual console has TIOCSTI-like
     * effects */
    {SCMP_SYS (ioctl), EPERM, &SCMP_A1 (SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, (int) TIOCLINUX)},

    /* seccomp cannot inspect clone3()'s struct clone_args to filter flags,
     * so block clone3() outright and let userspace fall back to clone().
     * (CVE-2021-41133) */
    {SCMP_SYS (clone3), ENOSYS},

    /* New mount-manipulation APIs can also change our VFS; block all of
     * them rather than reason about which are dangerous.
     * (CVE-2021-41133) */
    {SCMP_SYS (open_tree), ENOSYS},
    {SCMP_SYS (move_mount), ENOSYS},
    {SCMP_SYS (fsopen), ENOSYS},
    {SCMP_SYS (fsconfig), ENOSYS},
    {SCMP_SYS (fsmount), ENOSYS},
    {SCMP_SYS (fspick), ENOSYS},
    {SCMP_SYS (mount_setattr), ENOSYS},

    /* Profiling / debugging from inside the sandbox; perf in particular has
     * been the source of many CVEs. */
    {SCMP_SYS (perf_event_open), EPERM},
    /* Don't allow switching to bsd emulation or whatnot */
    {SCMP_SYS (personality), EPERM, &SCMP_A0 (SCMP_CMP_NE, (gulong) PER_LINUX)},
    {SCMP_SYS (ptrace), EPERM},
  };

  /* Socket-family allowlist. Anything not on this list is blocked with
   * EAFNOSUPPORT. */
  static const int socket_family_allowlist[] = {
    /* Keep in numerical order */
    AF_UNSPEC,
    AF_LOCAL,
    AF_INET,
    AF_INET6,
    AF_NETLINK,
  };
  int last_allowed_family;
  guint i;
  int r;
  int fd = -1;
  g_autofree char *fd_str = NULL;
  g_autofree char *path = NULL;

  seccomp = seccomp_init (SCMP_ACT_ALLOW);
  if (!seccomp)
    return seccomp_fail (error, "Initialize seccomp failed");

  /* Native arch is auto-added by seccomp_init; seccomp_arch_add returns
   * -EEXIST for it which we tolerate. The extra_arches loop adds the 32-bit
   * compat sibling so the filter also covers multiarch thumbnailers. */
  r = seccomp_arch_add (seccomp, arch_id);
  if (r < 0 && r != -EEXIST)
    return seccomp_fail (error, "Failed to add architecture to seccomp filter: %s",
                         seccomp_strerror (r));

  for (i = 0; seccomp_extra_arches[i] != 0; i++)
    {
      r = seccomp_arch_add (seccomp, seccomp_extra_arches[i]);
      if (r < 0 && r != -EEXIST)
        return seccomp_fail (error, "Failed to add multiarch architecture to seccomp filter: %s",
                             seccomp_strerror (r));
    }

  for (i = 0; i < G_N_ELEMENTS (syscall_blocklist); i++)
    {
      int scall = syscall_blocklist[i].scall;
      int errnum = syscall_blocklist[i].errnum;

      g_return_val_if_fail (errnum == EPERM || errnum == ENOSYS, FALSE);

      if (syscall_blocklist[i].arg)
        r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 1, *syscall_blocklist[i].arg);
      else
        r = seccomp_rule_add (seccomp, SCMP_ACT_ERRNO (errnum), scall, 0);

      /* EFAULT here typically means libseccomp can't map the syscall number
       * to a name and back on the non-native architecture. Non-fatal. */
      if (r == -EFAULT)
        g_debug ("Unable to block syscall %d: syscall not known to libseccomp?", scall);
      else if (r < 0)
        return seccomp_fail (error, "Failed to block syscall %d: %s",
                             scall, seccomp_strerror (r));
    }

  /* Socket-family filtering may not work on all arches; failures here are
   * non-fatal. Use seccomp_rule_add_exact to avoid libseccomp rewriting the
   * comparison. See https://github.com/seccomp/libseccomp/issues/8 */
  last_allowed_family = -1;
  for (i = 0; i < G_N_ELEMENTS (socket_family_allowlist); i++)
    {
      int family = socket_family_allowlist[i];
      int disallowed;

      for (disallowed = last_allowed_family + 1; disallowed < family; disallowed++)
        seccomp_rule_add_exact (seccomp, SCMP_ACT_ERRNO (EAFNOSUPPORT), SCMP_SYS (socket), 1, SCMP_A0 (SCMP_CMP_EQ, disallowed));
      last_allowed_family = family;
    }
  /* Block everything above the highest allowed family. */
  seccomp_rule_add_exact (seccomp, SCMP_ACT_ERRNO (EAFNOSUPPORT), SCMP_SYS (socket), 1, SCMP_A0 (SCMP_CMP_GE, last_allowed_family + 1));

  fd = g_file_open_tmp ("cinnamon-seccomp-XXXXXX", &path, error);
  if (fd == -1)
    return FALSE;

  g_unlink (path);

  if (seccomp_export_bpf (seccomp, fd) != 0)
    {
      close (fd);
      return seccomp_fail (error, "Failed to export bpf");
    }

  lseek (fd, 0, SEEK_SET);

  fd_str = g_strdup_printf ("%d", fd);
  if (fd_array)
    g_array_append_val (fd_array, fd);

  add_args (argv_array, "--seccomp", fd_str, NULL);

  /* Owned by fd_array now; don't close on success. */
  return TRUE;
}

#endif /* ENABLE_SECCOMP */

#ifdef HAVE_BWRAP

static gboolean
path_is_usrmerged (const char *dir)
{
  /* Does /dir point to /usr/dir? */
  g_autofree char *target = NULL;
  GStatBuf stat_buf_src, stat_buf_target;

  if (g_stat (dir, &stat_buf_src) < 0)
    return FALSE;

  target = g_strdup_printf ("/usr%s", dir);

  if (g_stat (target, &stat_buf_target) < 0)
    return FALSE;

  return (stat_buf_src.st_dev == stat_buf_target.st_dev) &&
         (stat_buf_src.st_ino == stat_buf_target.st_ino);
}

static void
add_bwrap_env (GPtrArray  *array,
               const char *envvar)
{
  if (g_getenv (envvar) != NULL)
    add_args (array, "--setenv", envvar, g_getenv (envvar), NULL);
}

static gboolean
add_bwrap (GPtrArray  *array,
           ScriptExec *script)
{
  const char * const usrmerged_dirs[] = { "bin", "lib64", "lib", "sbin" };
  int i;
  g_autofree char *gst_cache_dir = NULL;

  g_return_val_if_fail (script->outdir != NULL, FALSE);
  g_return_val_if_fail (script->s_infile != NULL, FALSE);

  add_args (array,
            "bwrap",
            "--ro-bind", "/usr", "/usr",
            "--ro-bind-try", "/etc/ld.so.cache", "/etc/ld.so.cache",
            NULL);

  /* These directories might be symlinks into /usr/... */
  for (i = 0; i < (int) G_N_ELEMENTS (usrmerged_dirs); i++)
    {
      g_autofree char *absolute_dir = g_strdup_printf ("/%s", usrmerged_dirs[i]);

      if (!g_file_test (absolute_dir, G_FILE_TEST_EXISTS))
        continue;

      if (path_is_usrmerged (absolute_dir))
        {
          g_autofree char *symlink_target = g_strdup_printf ("/usr/%s", usrmerged_dirs[i]);
          add_args (array, "--symlink", symlink_target, absolute_dir, NULL);
        }
      else
        {
          add_args (array, "--ro-bind", absolute_dir, absolute_dir, NULL);
        }
    }

  /* fontconfig cache if outside /usr */
  if (!g_str_has_prefix (FONTCONFIG_CACHE_PATH, "/usr/"))
    add_args (array, "--ro-bind-try", FONTCONFIG_CACHE_PATH, FONTCONFIG_CACHE_PATH, NULL);

  /* Persistent GStreamer plugin registry — without this, GStreamer-based
   * thumbnailers (e.g. totem-video-thumbnailer) rescan all plugins on every
   * invocation because $XDG_CACHE_HOME isn't bound into the sandbox. */
  gst_cache_dir = create_gst_cache_dir ();
  if (gst_cache_dir)
    {
      g_autofree char *registry = g_build_filename (gst_cache_dir, GST_REGISTRY_FILENAME, NULL);
      script->has_gst_registry = TRUE;
      add_args (array,
                "--setenv", "GST_REGISTRY_1_0", registry,
                "--bind", gst_cache_dir, gst_cache_dir,
                NULL);
    }

  /* update-alternatives targets — some /usr files chain through /etc/alternatives */
  add_args (array, "--ro-bind-try", "/etc/alternatives", "/etc/alternatives", NULL);

  add_args (array,
            "--proc", "/proc",
            "--dev", "/dev",
            "--chdir", "/",
            "--setenv", "GIO_USE_VFS", "local",
            "--unshare-all",
            "--die-with-parent",
            NULL);

  add_bwrap_env (array, "G_MESSAGES_DEBUG");
  add_bwrap_env (array, "G_MESSAGES_PREFIXED");
  add_bwrap_env (array, "GST_DEBUG");

  /* Bind the per-invocation output dir as /tmp inside the sandbox. */
  g_ptr_array_add (array, g_strdup ("--bind"));
  g_ptr_array_add (array, g_strdup (script->outdir));
  g_ptr_array_add (array, g_strdup ("/tmp"));

  /* Expose the input file under its sandbox path (extension preserved). */
  g_ptr_array_add (array, g_strdup ("--ro-bind"));
  g_ptr_array_add (array, g_strdup (script->infile));
  g_ptr_array_add (array, g_strdup (script->s_infile));

  return TRUE;
}

/* Check that bwrap is installed. We deliberately avoid running bwrap here
 * because g_spawn_sync from a worker thread deadlocks when another library
 * in the same process (gvfs, etc.) has a SIGCHLD reaper installed — it
 * reaps our child before our waitpid sees it. If bwrap is broken at
 * runtime (AppArmor unprivileged_userns_clone restrictions,
 * user.max_user_namespaces=0), the actual thumbnail spawn will exit
 * non-zero and we'll log it. */
static gboolean
bwrap_is_available (void)
{
  static gsize initialized = 0;
  static gboolean available = FALSE;

  if (g_once_init_enter (&initialized))
    {
      g_autofree char *path = g_find_program_in_path ("bwrap");
      available = (path != NULL);
      if (!available)
        g_warning ("bwrap not found in PATH; thumbnailers will run unsandboxed");
      g_once_init_leave (&initialized, 1);
    }

  return available;
}

#endif /* HAVE_BWRAP */

static char **
expand_thumbnailing_cmd (const char  *cmd,
                         ScriptExec  *script,
                         int          size,
                         const char  *mime_type,
                         GError     **error)
{
  GPtrArray *array;
  g_auto(GStrv) cmd_elems = NULL;
  guint i;
  gboolean got_in, got_out;

  if (!g_shell_parse_argv (cmd, NULL, &cmd_elems, error))
    return NULL;

  script->thumbnailer_name = g_strdup (cmd_elems[0]);

  array = g_ptr_array_new_with_free_func (g_free);

#ifdef HAVE_BWRAP
  if (script->use_sandbox)
    {
      if (!add_bwrap (array, script))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Bubblewrap setup failed");
          goto bail;
        }
    }
#endif

#ifdef ENABLE_SECCOMP
  if (script->use_sandbox)
    {
      if (!setup_seccomp (array, script->fd_array, error))
        goto bail;
    }
#endif

  got_in = got_out = FALSE;
  for (i = 0; cmd_elems[i] != NULL; i++)
    {
      char *expanded;

      expanded = expand_thumbnailing_elem (cmd_elems[i],
                                           size,
                                           mime_type,
                                           script->s_infile ? script->s_infile : script->infile,
                                           script->s_outfile ? script->s_outfile : script->outfile,
                                           &got_in,
                                           &got_out);

      g_ptr_array_add (array, expanded);
    }

  if (!got_in)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Input file could not be set");
      goto bail;
    }
  else if (!got_out)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Output file could not be set");
      goto bail;
    }

  g_ptr_array_add (array, NULL);

  return (char **) g_ptr_array_free (array, FALSE);

bail:
  g_ptr_array_free (array, TRUE);
  return NULL;
}

static void
child_setup (gpointer user_data)
{
  GArray *fd_array = user_data;
  guint i;

  if (fd_array == NULL)
    return;

  /* Mark inherited fds (seccomp BPF blob) not-close-on-exec. */
  for (i = 0; i < fd_array->len; i++)
    fcntl (g_array_index (fd_array, int, i), F_SETFD, 0);
}

static void
clear_fd (gpointer data)
{
  int *fd_p = data;
  if (fd_p != NULL && *fd_p != -1)
    close (*fd_p);
}

static void
script_exec_free (ScriptExec *exec)
{
  if (exec == NULL)
    return;

  g_free (exec->infile);
  if (exec->infile_tmp)
    {
      if (g_file_test (exec->infile_tmp, G_FILE_TEST_IS_DIR))
        g_rmdir (exec->infile_tmp);
      else
        g_unlink (exec->infile_tmp);
      g_free (exec->infile_tmp);
    }
  if (exec->outfile)
    {
      g_unlink (exec->outfile);
      g_free (exec->outfile);
    }
  if (exec->outdir)
    {
      if (g_rmdir (exec->outdir) < 0)
        g_warning ("Could not remove %s, thumbnailer %s left files in directory",
                   exec->outdir, exec->thumbnailer_name ? exec->thumbnailer_name : "(unknown)");
      g_free (exec->outdir);
    }
  g_free (exec->thumbnailer_name);
  g_free (exec->s_infile);
  g_free (exec->s_outfile);
  if (exec->fd_array)
    g_array_free (exec->fd_array, TRUE);
  g_free (exec);
}

static char *
get_extension (const char *path)
{
  g_autofree char *basename = NULL;
  const char *dot;

  basename = g_path_get_basename (path);
  dot = strrchr (basename, '.');
  if (dot == NULL || dot == basename)
    return NULL;
  return g_strdup (dot + 1);
}

static ScriptExec *
script_exec_new (const char  *uri,
                 GError     **error)
{
  ScriptExec *exec;
  g_autoptr(GFile) file = NULL;

  exec = g_new0 (ScriptExec, 1);
  exec->use_sandbox = FALSE;

#ifdef HAVE_BWRAP
  exec->use_sandbox = bwrap_is_available ();
#endif

  file = g_file_new_for_uri (uri);
  exec->infile = g_file_get_path (file);
  if (!exec->infile)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "Could not get path for URI '%s'", uri);
      goto bail;
    }

  if (exec->use_sandbox)
    {
      char *tmpl;
      g_autofree char *ext = NULL;
      g_autofree char *infile_name = NULL;

      exec->fd_array = g_array_new (FALSE, TRUE, sizeof (int));
      g_array_set_clear_func (exec->fd_array, clear_fd);

      tmpl = g_strdup ("/tmp/cinnamon-desktop-thumbnailer-XXXXXX");
      exec->outdir = g_mkdtemp (tmpl);
      if (!exec->outdir)
        {
          int errsv = errno;

          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       "Could not create temporary sandbox directory: %s",
                       g_strerror (errsv));
          g_free (tmpl);
          goto bail;
        }
      exec->outfile = g_build_filename (exec->outdir, "cinnamon-desktop-thumbnailer.png", NULL);

      ext = get_extension (exec->infile);
      if (ext)
        infile_name = g_strdup_printf ("cinnamon-desktop-file-to-thumbnail.%s", ext);
      else
        infile_name = g_strdup ("cinnamon-desktop-file-to-thumbnail");

      exec->s_infile = g_build_filename ("/tmp", infile_name, NULL);
      exec->s_outfile = g_build_filename ("/tmp", "cinnamon-desktop-thumbnailer.png", NULL);
      /* bwrap will create this empty stub when it binds infile onto s_infile. */
      exec->infile_tmp = g_build_filename (exec->outdir, infile_name, NULL);
    }
  else
    {
      int fd;
      g_autofree char *tmpname = NULL;

      fd = g_file_open_tmp (".cinnamon_desktop_thumbnail.XXXXXX", &tmpname, error);
      if (fd == -1)
        goto bail;
      close (fd);
      exec->outfile = g_steal_pointer (&tmpname);
    }

  return exec;

bail:
  script_exec_free (exec);
  return NULL;
}

/* The thumbnailer may have written transient plugin-scan artifacts into the
 * shared registry cache dir. Keep the registry file itself (that's the whole
 * point of the persistent cache) and remove everything else. */
static void
clean_gst_registry_dir (ScriptExec *exec)
{
  g_autoptr(GDir) dir = NULL;
  g_autofree char *gst_cache_dir = NULL;
  g_autoptr(GError) error = NULL;
  const char *name;

  if (!exec->has_gst_registry)
    return;

  gst_cache_dir = create_gst_cache_dir ();
  if (!gst_cache_dir)
    return;

  dir = g_dir_open (gst_cache_dir, 0, &error);
  if (!dir)
    {
      g_debug ("Failed to open GStreamer registry dir %s: %s",
               gst_cache_dir, error->message);
      return;
    }

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      g_autofree char *fullpath = NULL;

      if (g_strcmp0 (name, GST_REGISTRY_FILENAME) == 0)
        continue;

      fullpath = g_build_filename (gst_cache_dir, name, NULL);
      if (g_remove (fullpath) < 0)
        g_warning ("Failed to delete left-over thumbnailing artifact '%s'", fullpath);
      else
        g_debug ("Removed left-over file '%s' in '%s'", name, gst_cache_dir);
    }
}

static void
print_script_debug (GStrv expanded)
{
  g_autoptr(GString) out = NULL;
  guint i;

  if (!g_log_writer_default_would_drop (G_LOG_LEVEL_DEBUG, G_LOG_DOMAIN))
    {
      out = g_string_new (NULL);
      for (i = 0; expanded[i]; i++)
        g_string_append_printf (out, "%s ", expanded[i]);
      g_debug ("Spawning thumbnailer: %s", out->str);
    }
}

GBytes *
_gnome_desktop_thumbnail_script_exec (const char  *cmd,
                                      int          size,
                                      const char  *uri,
                                      const char  *mime_type,
                                      GError     **error)
{
  g_autofree char *error_out = NULL;
  g_auto(GStrv) expanded = NULL;
  int exit_status;
  gboolean ret;
  GBytes *image = NULL;
  ScriptExec *exec;

  exec = script_exec_new (uri, error);
  if (!exec)
    return NULL;

  expanded = expand_thumbnailing_cmd (cmd, exec, size, mime_type, error);
  if (expanded == NULL)
    goto out;

  print_script_debug (expanded);

  ret = g_spawn_sync (NULL, expanded, NULL, G_SPAWN_SEARCH_PATH,
                      child_setup, exec->fd_array, NULL, &error_out,
                      &exit_status, error);
  if (ret && g_spawn_check_wait_status (exit_status, error))
    {
      char *contents;
      gsize length;

      if (g_file_get_contents (exec->outfile, &contents, &length, error))
        image = g_bytes_new_take (contents, length);
    }
  else if (ret && error_out && *error_out)
    {
      /* Child exited non-zero; the GError from g_spawn_check_wait_status is
       * just "exited with code N" — prepend the child's stderr so the caller
       * sees e.g. bwrap's "setting up uid map: Permission denied". */
      g_prefix_error (error, "%s: ", g_strstrip (error_out));
    }

  clean_gst_registry_dir (exec);

out:
  script_exec_free (exec);
  return image;
}
