/* clippy.c
 *
 * Copyright 2018 Endless Mobile, Inc.
 *
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Author: Juan Pablo Ugarte <ugarte@endlessm.com>
 */

#include <gmodule.h>
#include <gtk/gtk.h>
#include "utils.h"
#include "clippy-dbus-wrapper.h"

#define HIGHLIGHT_CLASS "highlight"
#define DBUS_IFACE      "com.hack_computer.Clippy"
#define DBUS_OBJECT_PATH "/com/hack_computer/Clippy"
#define CLIPPY_TIMEOUT_KEY "ClippyTimeOut"

static GDBusInterfaceInfo *iface_info = NULL;

typedef struct
{
  GDBusConnection *connection; /* DBus connection */
  GtkCssProvider *provider; /* Clippy Css provider */
  GHashTable     *widgets;  /* Highlighted widget */

  GHashTable     *messages; /* ShowMessage GtkPopover table */

  GClosure       *signal_closure;
  GClosure       *notify_closure;

  gchar          *css;

  GDBusObjectManagerServer *manager;
} Clippy;

static inline void
clippy_emit_signal (Clippy      *clip,
                    const gchar *signal_name,
                    const gchar *format,
                    ...)
{
  va_list params;

  va_start (params, format);
  g_dbus_connection_emit_signal (clip->connection,
                                 NULL,
                                 DBUS_OBJECT_PATH,
                                 DBUS_IFACE,
                                 signal_name,
                                 g_variant_new_va (format, NULL, &params),
                                 NULL);
  va_end (params);
}

static void
signal_closure_callback (void)
{
  g_debug ("%s", __func__);
}

static void
signal_closure_marshall (GClosure     *closure,
                         GValue       *return_value,
                         guint         n_param_values,
                         const GValue *param_values,
                         gpointer      invocation_hint,
                         gpointer      marshal_data)
{
  Clippy *clip = marshal_data;
  GSignalInvocationHint *hint = invocation_hint;
  GVariantBuilder builder;
  GObject *object = NULL;
  
  if (!n_param_values || !G_VALUE_HOLDS_OBJECT (param_values))
    return;
  
  object = g_value_get_object (param_values);

  g_debug ("%s %s %s %s %d", __func__,
           G_OBJECT_TYPE_NAME (object), 
           object_get_name (object),
           g_signal_name (hint->signal_id),
           n_param_values);

  /*
   * DBus does not support maybe types or empty tuples this is why we
   * include the object name in the parameters tuple
   */
  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
      
  /* Add extra parameters (including instance) */
  for (guint i = 0; i < n_param_values; i++)
    g_variant_builder_add_value (&builder, variant_new_value (&param_values[i]));

  /* Emit D-Bus signal */
  clippy_emit_signal (clip,
                      "ObjectSignal",
                      "(ssv)",
                      g_signal_name (hint->signal_id),
                      hint->detail ? g_quark_to_string (hint->detail) : "",
                      g_variant_builder_end (&builder));
}

static void
notify_closure_callback (GObject    *gobject,
                         GParamSpec *pspec,
                         Clippy     *clip)
{
  const gchar *id  = object_get_name (gobject);
  g_auto(GValue) value = G_VALUE_INIT;

  g_debug ("%s %s %s", __func__, id, pspec->name);
  
  g_value_init (&value, pspec->value_type);
  g_object_get_property (gobject, pspec->name, &value);

  /* Emit D-Bus signal */
  clippy_emit_signal (clip,
                      "ObjectNotify",
                      "(ssv)",
                      id,
                      pspec->name,
                      variant_new_value (&value));
}

static Clippy *
clippy_new (GDBusConnection *connection)
{
  Clippy *clip = g_new0 (Clippy, 1);

  clip->connection = g_object_ref (connection);
  clip->provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (clip->provider, "/com/endlessm/clippy/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), 
                                             GTK_STYLE_PROVIDER (clip->provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* id -> GtkWidget table */
  clip->widgets = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);

  /* Message id -> GtkPopover table */
  clip->messages = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);

  clip->signal_closure = g_cclosure_new (signal_closure_callback, NULL, NULL);
  g_closure_set_marshal (clip->signal_closure, signal_closure_marshall);
  g_closure_set_meta_marshal (clip->signal_closure, clip, signal_closure_marshall);
  g_closure_ref (clip->signal_closure);
  g_closure_sink (clip->signal_closure);

  clip->notify_closure = g_cclosure_new (G_CALLBACK (notify_closure_callback), clip, NULL);
  g_closure_ref (clip->notify_closure);
  g_closure_sink (clip->notify_closure);
  
  clip->manager = g_dbus_object_manager_server_new (DBUS_OBJECT_PATH);
  g_dbus_object_manager_server_set_connection (clip->manager, connection);

  return clip;
}

static void
clippy_free (Clippy *clip)
{
  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (clip->provider));
  g_clear_object (&clip->connection);
  g_clear_object (&clip->provider);
  g_clear_object (&clip->manager);
  g_clear_pointer (&clip->widgets, g_hash_table_unref);
  g_clear_pointer (&clip->messages, g_hash_table_unref);
  g_clear_pointer (&clip->signal_closure, g_closure_unref);
  g_clear_pointer (&clip->notify_closure, g_closure_unref);
  g_clear_pointer (&clip->css, g_free);
}

static void
clippy_timeout_add (GObject     *object,
                    guint        timeout,
                    GSourceFunc  function)
{
  gpointer old_id;
  guint id;

  if (!timeout)
    return;

  /* Clear old timeout */
  if ((old_id = g_object_get_data (object, CLIPPY_TIMEOUT_KEY)))
    g_source_remove (GPOINTER_TO_UINT (old_id));

  /* Add new timeout */
  id = g_timeout_add (timeout, function, object);

  /* Save new timeout */
  g_object_set_data (object, CLIPPY_TIMEOUT_KEY, GUINT_TO_POINTER (id));
}

static void
clippy_timeout_remove (GObject *object, gboolean clear_source)
{
  gpointer timeout;

  if (clear_source && (timeout = g_object_get_data (object, CLIPPY_TIMEOUT_KEY)))
    g_source_remove (GPOINTER_TO_UINT (timeout));

  g_object_set_data (object, CLIPPY_TIMEOUT_KEY, NULL);
}

static gboolean
clippy_highlight_clear (gpointer data)
{
  clippy_timeout_remove (data, FALSE);
  gtk_style_context_remove_class (gtk_widget_get_style_context (data),
                                  HIGHLIGHT_CLASS);
  return G_SOURCE_REMOVE;
}

static void
clippy_highlight (Clippy       *clip,
                  const gchar  *object,
                  guint         timeout,
                  GError      **error)
{
  GObject *gobject;

  /* Find widget */
  if (!app_get_object_info (object, NULL, NULL,
                            &gobject, NULL, NULL, error))
    return;

  clippy_return_if_fail (GTK_IS_WIDGET (gobject),
                         error, CLIPPY_NOT_A_WIDGET,
                         "Object '%s' of type %s is not a GtkWidget",
                         object,
                         G_OBJECT_TYPE_NAME (gobject));

  /* Insert widget in table */
  g_hash_table_insert (clip->widgets, g_strdup (object), g_object_ref (gobject));

  g_debug ("%s %s %p", __func__, object, (gpointer) gobject);

  /* Highlight */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (gobject)),
                               HIGHLIGHT_CLASS);

  clippy_timeout_add (gobject, timeout, clippy_highlight_clear);
}

static void
clippy_unhighlight (Clippy *clip, const gchar *object, GError **error)
{
  GObject *gobject;

  if (!app_get_object_info (object, NULL, NULL,
                            &gobject, NULL, NULL, error))
    return;

  clippy_timeout_remove (gobject, TRUE);

  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (gobject)),
                                  HIGHLIGHT_CLASS);
  g_hash_table_remove (clip->widgets, object);
}

static void
on_popover_closed (GtkPopover *popover, Clippy *clip)
{
  const gchar *id;

  clippy_timeout_remove (G_OBJECT (popover), TRUE);

  if ((id = gtk_widget_get_name (GTK_WIDGET (popover))))
    {
      clippy_emit_signal (clip, "MessageDone", "(s)", id);
      g_hash_table_remove (clip->messages, id);
    }
}

static gboolean
clippy_popover_clear (gpointer data)
{
  clippy_timeout_remove (data, FALSE);
  gtk_popover_popdown (data);
  return G_SOURCE_REMOVE;
}

static void
clippy_message (Clippy       *clip,
                const gchar  *id,
                const gchar  *text,
                const gchar  *image,
                const gchar  *relative_to,
                guint         timeout,
                GError      **error)
{
  GtkWidget *box, *popover;

  if ((popover = g_hash_table_lookup (clip->messages, id)))
    {
      /* Reuse existing popover */
      gtk_container_remove (GTK_CONTAINER (popover),
                            gtk_bin_get_child (GTK_BIN (popover)));
    }
  else
    {
      GObject *gobject;

      if (!app_get_object_info (relative_to, NULL, NULL,
                                &gobject, NULL, NULL, error))
        return;

      clippy_return_if_fail (GTK_IS_WIDGET (gobject),
                             error, CLIPPY_NOT_A_WIDGET,
                             "Object '%s' of type %s is not a GtkWidget",
                             relative_to,
                             G_OBJECT_TYPE_NAME (gobject));

      /* Create new popover */
      popover = gtk_popover_new (GTK_WIDGET (gobject));
      gtk_widget_set_name (GTK_WIDGET (popover), id);

      g_hash_table_insert (clip->messages,
                           g_strdup (id),
                           g_object_ref_sink (popover));

      g_signal_connect (popover, "closed",
                        G_CALLBACK (on_popover_closed),
                        clip);
    }

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  g_object_set (box, "margin", 8, NULL);
  gtk_container_add (GTK_CONTAINER (popover), box);

  if (*image != '\0')
    {
      GtkWidget *img = gtk_image_new ();
      gtk_image_set_from_icon_name (GTK_IMAGE (img), image, GTK_ICON_SIZE_DIALOG);
      gtk_box_pack_start (GTK_BOX (box), img, FALSE, TRUE, 0);
    }

  if (*text != '\0')
    {
      GtkWidget *label = gtk_label_new (text);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
    }

  clippy_timeout_add (G_OBJECT(popover), timeout, clippy_popover_clear);

  gtk_widget_show_all (box);
  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
clippy_message_clear (Clippy *clip, const gchar *id, GError **error)
{
  GtkWidget *popover;

  clippy_return_if_fail ((popover = g_hash_table_lookup (clip->messages, id)),
                         error, CLIPPY_WRONG_MSG_ID,
                         "Message id '%s' not found",
                         id);

  gtk_popover_popdown (GTK_POPOVER (popover));
}

static void
clippy_set (Clippy       *clip,
            const gchar  *object,
            const gchar  *property,
            GVariant     *variant,
            GError      **error)
{
  g_auto(GValue) gvalue = G_VALUE_INIT;
  GObject *gobject;
  GParamSpec *pspec;
  
  g_debug ("%s %s %s", __func__, object, property);
  
  if (!app_get_object_info (object, property, NULL,
                            &gobject, &pspec, NULL, error))
    return;

  g_value_init (&gvalue, pspec->value_type);
  value_set_variant (&gvalue, variant);

  g_object_set_property (gobject, property, &gvalue);
}

static void
clippy_get (Clippy       *clip,
            const gchar  *object,
            const gchar  *property,
            GVariant    **return_value,
            GError      **error)
{
  g_auto(GValue) gvalue = G_VALUE_INIT;
  GObject *gobject;
  GParamSpec *pspec;
  
  g_debug ("%s %s %s", __func__, object, property);

  if (!app_get_object_info (object, property, NULL,
                            &gobject, &pspec, NULL, error))
    return;

  g_value_init (&gvalue, pspec->value_type);
  g_object_get_property (gobject, property, &gvalue);

  if (return_value)
    *return_value = g_variant_new ("(v)", variant_new_value (&gvalue));
}

static void
clippy_connect (Clippy       *clip,
                const gchar  *object,
                const gchar  *signal,
                const gchar  *detail,
                GError      **error)
{
  GObject *gobject;
  GClosure *closure;
  gboolean notify;
  GQuark quark;
  guint id;

  g_debug ("%s %s %s %s", __func__, object, signal, detail ? detail : "null");

  if (!app_get_object_info (object, NULL, signal,
                            &gobject, NULL, &id, error))
    return;

  notify = g_strcmp0 (signal, "notify") == 0;
  quark = g_quark_from_string (detail);
  
  if (notify)
    clippy_return_if_fail (quark,
                           error, CLIPPY_NO_DETAIL,
                           "Notify signal for object '%s' requieres detail (property)",
                           object);
  
  closure = (notify) ? clip->notify_closure : clip->signal_closure;
  g_signal_connect_closure_by_id (gobject, id, quark, closure, FALSE);
}

static void
clippy_emit (Clippy       *clip,
             const gchar  *signal,
             const gchar  *detail,
             GVariant     *params,
             GError      **error)
{
  g_autoptr(GVariant) variant = NULL;
  const gchar *object;
  GSignalQuery query;
  GObject *gobject;
  guint id;

  if (!params ||
      !(variant = g_variant_get_child_value (params, 0)) ||
      !(object = g_variant_get_string (variant, NULL)) ||
      !app_get_object_info (object, NULL, signal,
                            &gobject, NULL, &id, error))
    return;

  g_signal_query (id, &query);

  g_debug ("%s is action %s %s n_params %d n_children %ld", __func__,
           object,
           signal,
           query.n_params,
           (params) ? g_variant_n_children (params) : 0);

  object_emit_action_signal (gobject, &query, detail, params, error);
}

static void
clippy_export (Clippy       *clip,
               const gchar  *object,
               GVariant    **return_value,
               GError      **error)
{
  g_autoptr(GDBusObjectSkeleton) skeleton = NULL;
  g_autoptr(ClippyDbusWrapper) interface = NULL;
  g_autofree gchar *object_path = NULL;
  g_autoptr(GString) node_info = NULL;
  GDBusInterfaceInfo *info;
  GObject *gobject;

  g_debug ("%s %s", __func__, object);

  if (!app_get_object_info (object, NULL, NULL, &gobject, NULL, NULL, error))
    return;

  object_path = g_build_path ("/", DBUS_OBJECT_PATH, "objects", object, NULL);

  /* Replace dots with slashes */
  str_replace_char (object_path, '.', '/');

  skeleton = g_dbus_object_skeleton_new (object_path);
  clippy_return_if_fail (skeleton,
                         error, CLIPPY_NO_OBJECT,
                         "Could not create dbus skeleton for object '%s'",
                         object);

  interface = clippy_dbus_wrapper_new (gobject, object_path);

  /* We need to increment  the reference count of the dynamic info struct since
   * DBus will try to unref it when unexporting it!
   */
  info = g_dbus_interface_skeleton_get_info (G_DBUS_INTERFACE_SKELETON (interface));
  g_dbus_interface_info_ref (info);

  g_dbus_object_skeleton_add_interface (skeleton, G_DBUS_INTERFACE_SKELETON (interface));
  g_dbus_object_manager_server_export (clip->manager, skeleton);

  clippy_return_if_fail (g_dbus_object_manager_server_is_exported(clip->manager, skeleton),
                         error, CLIPPY_NO_OBJECT,
                         "Could not export object '%s' on dbus path '%s'",
                         object,
                         g_dbus_object_get_object_path (G_DBUS_OBJECT (skeleton)));

  node_info = g_string_new ("<node xmlns:doc=\"http://www.freedesktop.org/dbus/1.0/doc.dtd\">\n");
  g_dbus_interface_info_generate_xml (info, 2, node_info);
  g_string_append (node_info,"</node>");

  if (return_value)
    *return_value = g_variant_new ("(ss)", object_path, node_info->str);
}

static void
clippy_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  GVariant *return_value = NULL;
  Clippy *clip = user_data;
  GError *error = NULL;
  GApplication *app;

  /* Make sure the app is activated in case it was autostarted
   * through this method call.
   */
  app = g_application_get_default ();
  if (app && !gtk_application_get_active_window (GTK_APPLICATION (app)))
    g_application_activate (app);

  if (g_strcmp0 (method_name, "Highlight") == 0)
    {
      g_autofree gchar *object = NULL;
      guint timeout;

      g_variant_get (parameters, "(su)", &object, &timeout);
      clippy_highlight (clip, object, timeout, &error);
    }
  else if (g_strcmp0 (method_name, "Unhighlight") == 0)
    {
      g_autofree gchar *object = NULL;

      g_variant_get (parameters, "(s)", &object);
      clippy_unhighlight (clip, object, &error);
    }
  else if (g_strcmp0 (method_name, "Message") == 0)
    {
      g_autofree gchar *id = NULL, *text = NULL, *image = NULL, *relative_to = NULL;
      guint timeout;

      g_variant_get (parameters, "(ssssu)", &id, &text, &image, &relative_to, &timeout);
      clippy_message (clip, id, text, image, relative_to, timeout, &error);
    }
  if (g_strcmp0 (method_name, "MessageClear") == 0)
    {
      g_autofree gchar *id = NULL;
      g_variant_get (parameters, "(s)", &id);
      clippy_message_clear (clip, id, &error);
    }
  else if (g_strcmp0 (method_name, "Set") == 0)
    {
      g_autofree gchar *object = NULL, *property = NULL;
      g_autoptr(GVariant) value = NULL;

      g_variant_get (parameters, "(ssv)", &object, &property, &value);
      clippy_set (clip, object, property, value, &error);
    }
  else if (g_strcmp0 (method_name, "Get") == 0)
    {
      g_autofree gchar *object = NULL, *property = NULL;

      g_variant_get (parameters, "(ss)", &object, &property);
      clippy_get (clip, object, property, &return_value, &error);
    }
  else if (g_strcmp0 (method_name, "Connect") == 0)
    {
      g_autofree gchar *object = NULL, *signal = NULL, *detail = NULL;

      g_variant_get (parameters, "(sss)", &object, &signal, &detail);
      clippy_connect (clip, object, signal, detail, &error);
    }
  else if (g_strcmp0 (method_name, "Emit") == 0)
    {
      g_autofree gchar *signal = NULL, *detail = NULL;
      g_autoptr(GVariant) params = NULL;

      g_variant_get (parameters, "(ssv)", &signal, &detail, &params);
      clippy_emit (clip, signal, detail, params, &error);
    }
  else if (g_strcmp0 (method_name, "Export") == 0)
    {
      g_autofree gchar *object;

      g_variant_get (parameters, "(s)", &object);
      clippy_export (clip, object, &return_value, &error);
    }

  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    g_dbus_method_invocation_return_value (invocation, return_value);
}

static gboolean
clippy_set_property (GDBusConnection *connection,
                     const gchar     *sender,
                     const gchar     *object_path,
                     const gchar     *interface_name,
                     const gchar     *property_name,
                     GVariant        *value,
                     GError         **error,
                     gpointer         user_data)
{
  Clippy *clip = user_data;

  if (g_strcmp0 (property_name, "Css") == 0)
    {
      g_clear_pointer (&clip->css, g_free);
      g_variant_get (value, "s", &clip->css);
      gtk_css_provider_load_from_data (clip->provider, clip->css, -1, NULL);
    }
  else
    return FALSE;
  
  return TRUE;
}

static void
register_clippy_iface (void)
{
  static const GDBusInterfaceVTable vtable = {
    .method_call = clippy_method_call,
    .set_property = clippy_set_property
  };

  g_autoptr(GError) error = NULL;
  g_autoptr(GDBusConnection) connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                                          NULL,
                                                          &error);
  if (error)
    {
      g_critical ("Failed to get a session bus connection: %s", error->message);
      return;
    }

  g_dbus_connection_register_object (connection,
                                     DBUS_OBJECT_PATH,
                                     iface_info,
                                     &vtable,
                                     clippy_new (connection),
                                     (GDestroyNotify) clippy_free,
                                     &error);

  if (error)
    g_critical ("Failed to register Clippy object on connection: %s", error->message);
}

G_MODULE_EXPORT void
gtk_module_init(gint *argc, gchar ***argv)
{
  GDBusNodeInfo *info;
  GError *error = NULL;
  GBytes *xml;
  
  gtk_init_check (0, NULL);

  if ((xml = g_resources_lookup_data ("/com/endlessm/clippy/dbus.xml", 0, NULL)) &&
      (info = g_dbus_node_info_new_for_xml (g_bytes_get_data (xml, NULL), &error)))
    {
      iface_info = g_dbus_node_info_lookup_interface (info, DBUS_IFACE);
      g_assert (iface_info != NULL);
      g_dbus_interface_info_ref (iface_info);
      g_dbus_node_info_unref (info);

      register_clippy_iface ();
    }
  else
    g_debug ("%s %s", __func__, error->message);
}

G_MODULE_EXPORT const gchar*
g_module_check_init(GModule *module) {
  g_module_make_resident(module);
  return NULL;
}
