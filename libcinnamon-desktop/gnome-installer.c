/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gnome-installer.h"

#define INSTALL_DBUS_PATH "org.freedesktop.PackageKit.Modify"
#define CHECK_DBUS_PATH "org.freedesktop.PackageKit.Query"

typedef enum
{
    OP_TYPE_INSTALL,
    OP_TYPE_CHECK
} OpType;

typedef struct
{
  OpType type;
  gchar **packages;
  gchar *options;

  GSimpleAsyncResult *result;

} ClientCtx;

typedef struct
{
  GnomeInstallerClientCallback client_callback;
  gpointer client_data;
} ClientCallbackData;

static ClientCtx *
ctx_new (OpType              type,
         const gchar       **packages,
         const gchar        *options,
         GSimpleAsyncResult *result)
{
  ClientCtx *ctx = g_slice_new (ClientCtx);

  ctx->type = type;
  ctx->packages = g_strdupv ((gchar **) packages);
  ctx->options = g_strdup (options);
  ctx->result = g_object_ref (result);
  return ctx;
}

static void
ctx_free (ClientCtx *ctx)
{
  g_free (ctx->packages);
  g_free (ctx->options);
  g_object_unref (ctx->result);
  g_slice_free (ClientCtx, ctx);
}

static void
ctx_complete (ClientCtx *ctx)
{
  g_simple_async_result_complete (ctx->result);
  ctx_free (ctx);
}

static void
ctx_failed (ClientCtx *ctx,
            GError    *error)
{
  g_simple_async_result_take_error (ctx->result, error);
  ctx_complete (ctx);
}

static void
install_package_names_cb (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  ClientCtx *ctx = user_data;
  GError *error = NULL;
  GVariant *res;

  res = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), result, &error);
  if (res == NULL)
    {
      ctx_failed (ctx, error);
      return;
    }

  ctx_complete (ctx);

  g_variant_unref (res);
}

static void
check_for_packages_cb (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  ClientCtx *ctx = g_task_get_task_data (G_TASK (result));
  GError *error = NULL;

  G_GNUC_UNUSED gboolean success = g_task_propagate_boolean (G_TASK (result), &error);

  if (error != NULL) {
    ctx_failed (ctx, error);
    return;
  }

  ctx_complete (ctx);
}

static void
check_for_packages_thread (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);
  ClientCtx *ctx = (ClientCtx *) task_data;

  gboolean satisfied = TRUE;
  gint i = 0;
  GError *error = NULL;

  for (i = 0; i < g_strv_length (ctx->packages); i++) {
    GVariant *res = g_dbus_proxy_call_sync (proxy,
                                            "IsInstalled",
                                            g_variant_new ("(ss)", ctx->packages[i], ctx->options),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            G_MAXINT, NULL, &error);
    if (error != NULL) {
      satisfied = FALSE;
      break;
    }

    gboolean is_installed = FALSE;
    g_variant_get (res, "(b)", &is_installed);
    g_variant_unref (res);

    if (!is_installed) {
      satisfied = FALSE;
      break;
    }
  }

  if (error != NULL) {
    g_task_return_error (task, error);
    return;
  }

  if (!satisfied) {
    g_task_return_error (task,
                         g_error_new (g_quark_from_static_string ("GnomeInstaller"),
                         1,
                         "%s", ctx->packages[i]));
    return;
  }

  g_task_return_boolean (task, TRUE);
}

static void
check_for_packages (GDBusProxy *proxy, ClientCtx *ctx)
{
  GTask *task;

  task = g_task_new (proxy, NULL,
                     check_for_packages_cb,
                     NULL);

  g_task_set_task_data (task, ctx, NULL);

  g_task_run_in_thread (task, check_for_packages_thread);

  g_object_unref (task);
}

static void
pkg_kit_proxy_new_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  ClientCtx *ctx = user_data;
  GError *error = NULL;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_finish (result, &error);
  if (proxy == NULL)
    {
      ctx_failed (ctx, error);
      return;
    }

  if (ctx->type == OP_TYPE_INSTALL)
      g_dbus_proxy_call (proxy, "InstallPackageNames",
          g_variant_new ("(u^a&ss)", 0, ctx->packages, ctx->options),
          G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL, install_package_names_cb, ctx);
  else
      check_for_packages (proxy, ctx);

  g_object_unref (proxy);
}

static void
gnome_installer_run_op_async (const gchar        **packages,
                              const gchar         *options,
                              OpType               type,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  ClientCtx *ctx;
  GSimpleAsyncResult *result;

  result = g_simple_async_result_new (NULL, callback, user_data,
                                      gnome_installer_run_op_async);

  ctx = ctx_new (type, packages, options != NULL ? options : "", result);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE, NULL,
                            "org.freedesktop.PackageKit",
                            "/org/freedesktop/PackageKit",
                            type == OP_TYPE_INSTALL ? INSTALL_DBUS_PATH : CHECK_DBUS_PATH,
                            NULL, pkg_kit_proxy_new_cb, ctx);

  g_object_unref (result);
}

static gboolean
gnome_installer_async_finish (GAsyncResult *result,
                              GError      **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (result, NULL,
        gnome_installer_run_op_async), FALSE);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
        error))
    return FALSE;

  return TRUE;
}

static void
install_operation_async_cb (GObject      *source,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ClientCallbackData *data = (ClientCallbackData *) user_data;

  GError *error = NULL;

  if (!gnome_installer_async_finish (result, &error))
    {
      g_printerr ("Failed to install packages: %s\n", error->message);
      g_error_free (error);

      if (data->client_callback)
        (* data->client_callback) (FALSE, data->client_data);

      goto out;
    }

  if (data->client_callback)
    (* data->client_callback) (TRUE, data->client_data);

out:
  g_slice_free (ClientCallbackData, data);
}

static void
check_operation_async_cb (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  ClientCallbackData *data = (ClientCallbackData *) user_data;

  GError *error = NULL;

  if (!gnome_installer_async_finish (result, &error))
    {
      g_printerr ("Package missing: %s\n", error->message);
      g_error_free (error);

      if (data->client_callback)
        (* data->client_callback) (FALSE, data->client_data);

      goto out;
    }

  if (data->client_callback)
    (* data->client_callback) (TRUE, data->client_data);

out:
  g_slice_free (ClientCallbackData, data);
}


/**************************************************************/
/* Public API                                                 */
/**************************************************************/

/**
 * gnome_installer_install_packages:
 * @packages: (array zero-terminated=1): a null-terminated array of package names
 * @callback: (scope async): the callback to run for the result
 * @user_data: (closure): extra data to be sent to the callback
 *
 * Uses packagekit to install the provided list of packages.
 **/
void gnome_installer_install_packages (const gchar                 **packages, 
                                       GnomeInstallerClientCallback  callback,
                                       gpointer                      user_data)
{
  ClientCallbackData *data = g_slice_new (ClientCallbackData);

  data->client_callback = callback;
  data->client_data = user_data;

  gnome_installer_run_op_async (packages, NULL, OP_TYPE_INSTALL, install_operation_async_cb, data);
}

/**
 * gnome_installer_check_for_packages:
 * @packages: (array zero-terminated=1): a null-terminated array of package names
 * @callback: (scope async): the callback to run for the result
 * @user_data: (closure): extra data to be sent to the callback
 *
 * Uses packagekit to check if provided package names are installed.
 **/
void gnome_installer_check_for_packages (const gchar                **packages,
                                         GnomeInstallerClientCallback callback,
                                         gpointer                     user_data)
{
  ClientCallbackData *data = g_slice_new (ClientCallbackData);

  data->client_callback = callback;
  data->client_data = user_data;

  gnome_installer_run_op_async (packages, NULL, OP_TYPE_CHECK, check_operation_async_cb, data);
}


