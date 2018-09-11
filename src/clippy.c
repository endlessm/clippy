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

#include <gtk/gtk.h>
#include "utils.h"

#define HIGHLIGHT_CLASS "highlight"
#define DBUS_IFACE      "com.endlessm.Clippy"

static GDBusInterfaceInfo *iface_info = NULL;

typedef struct
{
  GApplication   *app;      /* Default Application */
  GtkCssProvider *provider; /* Clippy Css provider */
  GtkWidget      *widget;   /* Highlighted widget */

  GHashTable     *messages; /* ShowMessage GtkPopover table */

  GClosure       *signal_closure;
  GClosure       *notify_closure;

  gchar          *css;
} Clippy;

static inline void
clippy_emit_signal (Clippy      *clip,
                    const gchar *signal_name,
                    const gchar *format,
                    ...)
{
  va_list params;

  va_start (params, format);
  g_dbus_connection_emit_signal (g_application_get_dbus_connection (clip->app),
                                 NULL,
                                 g_application_get_dbus_object_path (clip->app),
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
  gint i;
  
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
  for (i = 0; i < n_param_values; i++)
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
  g_auto (GValue) value = G_VALUE_INIT;

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
clippy_new (GApplication *app)
{
  Clippy *clip = g_new0 (Clippy, 1);

  clip->app = app;
  clip->provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (clip->provider, "/com/endlessm/clippy/style.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (), 
                                             GTK_STYLE_PROVIDER (clip->provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

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
  
  return clip;
}

static void
clippy_free (Clippy *clip)
{
  g_clear_object (&clip->widget);

  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (clip->provider));
  g_clear_object (&clip->provider);
  g_clear_pointer (&clip->messages, g_hash_table_unref);
  g_clear_pointer (&clip->signal_closure, g_closure_unref);
  g_clear_pointer (&clip->notify_closure, g_closure_unref);
  g_clear_pointer (&clip->css, g_free);
}

static void
clippy_clear (Clippy *clip)
{
  if (clip->widget)
    gtk_style_context_remove_class (gtk_widget_get_style_context (clip->widget),
                                    HIGHLIGHT_CLASS);
  g_clear_object (&clip->widget);
}

static void
clippy_highlight (Clippy *clip, const gchar *object, GError **error)
{
  GObject *gobject;

  /* Dehighlight previous widget */
  clippy_clear (clip);
 
  /* Find and highlight widget */
  if (app_get_object_info (clip->app, object, NULL, NULL,
                           &gobject, NULL, NULL, error))
    return;

  clippy_return_if_fail (GTK_IS_WIDGET (gobject),
                         CLIPPY_NOT_A_WIDGET,
                         "Object '%s' of type %s is not a GtkWidget",
                         object,
                         G_OBJECT_TYPE_NAME (gobject));
  
  clip->widget = (GtkWidget *) g_object_ref (gobject);

  g_debug ("%s %s %p", __func__, object, clip->widget);
  
  gtk_style_context_add_class (gtk_widget_get_style_context (clip->widget),
                               HIGHLIGHT_CLASS);
}

static void
on_popover_closed (GtkPopover *popover, Clippy *clip)
{
  gpointer timeout;
  const gchar *id;

  if ((timeout = g_object_get_data (G_OBJECT (popover), "ClippyTimeOut")))
    {
      g_source_remove (GPOINTER_TO_UINT (timeout));
      g_object_set_data (G_OBJECT (popover), "ClippyTimeOut", NULL);
    }

  if ((id = gtk_widget_get_name (GTK_WIDGET (popover))))
    {
      clippy_emit_signal (clip, "MessageDone", "(s)", id);
      g_hash_table_remove (clip->messages, id);
    }
}

static gboolean
clippy_popover_clear (gpointer data)
{
  g_object_set_data (data, "ClippyTimeOut", NULL);
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

      if (app_get_object_info (clip->app, relative_to, NULL, NULL,
                               &gobject, NULL, NULL, error))
        return;

      clippy_return_if_fail (GTK_IS_WIDGET (gobject),
                             CLIPPY_NOT_A_WIDGET,
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

  if (timeout)
    {
      guint id = g_timeout_add (timeout, clippy_popover_clear, popover);
      gpointer old_id;

      /* Clear old timeout */
      if ((old_id = g_object_get_data (G_OBJECT (popover), "ClippyTimeOut")))
        g_source_remove (GPOINTER_TO_UINT (old_id));

      /* Save new timeout */
      g_object_set_data (G_OBJECT (popover), "ClippyTimeOut",
                         GUINT_TO_POINTER (id));
    }

  gtk_widget_show_all (box);
  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
clippy_message_clear (Clippy *clip, const gchar *id, GError **error)
{
  GtkWidget *popover;

  clippy_return_if_fail ((popover = g_hash_table_lookup (clip->messages, id)),
                         CLIPPY_WRONG_MSG_ID,
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
  g_auto (GValue) gvalue = G_VALUE_INIT;
  GObject *gobject;
  GParamSpec *pspec;
  
  g_debug ("%s %s %s", __func__, object, property);
  
  if (app_get_object_info (clip->app, object, property, NULL,
                           &gobject, &pspec, NULL, error))
    return;

  g_value_init (&gvalue, pspec->value_type);
  value_set_variant (&gvalue, variant, clip->app);

  g_object_set_property (gobject, property, &gvalue);
}

static void
clippy_get (Clippy       *clip,
            const gchar  *object,
            const gchar  *property,
            GVariant    **return_value,
            GError      **error)
{
  g_auto (GValue) gvalue = G_VALUE_INIT;
  GObject *gobject;
  GParamSpec *pspec;
  
  g_debug ("%s %s %s", __func__, object, property);

  if (app_get_object_info (clip->app, object, property, NULL,
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

  if (app_get_object_info (clip->app, object, NULL, signal,
                           &gobject, NULL, &id, error))
    return;

  notify = g_strcmp0 (signal, "notify") == 0;
  quark = g_quark_try_string (detail);
  
  if (notify)
    clippy_return_if_fail (quark,
                           CLIPPY_NO_DETAIL,
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
  g_auto (GValue) retval = G_VALUE_INIT;
  g_autoptr (GVariant) variant = NULL;
  g_autoptr (GArray) values = NULL;
  GValue *instance_and_params;
  const gchar *object;
  GSignalQuery query;
  GObject *gobject;
  guint id, i;

  if (!params ||
      !(variant = g_variant_get_child_value (params, 0)) ||
      !(object = g_variant_get_string (variant, NULL)) ||
      app_get_object_info (clip->app, object, NULL, signal,
                           &gobject, NULL, &id, error))
    return;

  g_signal_query (id, &query);

  /* We only support emiting action signals! */
  clippy_return_if_fail (query.signal_flags & G_SIGNAL_ACTION,
                         CLIPPY_WRONG_SIGNAL_TYPE,
                         "Can not emit signal '%s' from object '%s' of type %s because is not an action signal",
                         signal, object, G_OBJECT_TYPE_NAME (gobject));

  g_debug ("%s is action %s %s n_params %d n_children %ld", __func__,
           object,
           signal,
           query.n_params,
           (params) ? g_variant_n_children (params) : 0);

  /* Create values array */
  values = g_array_new (FALSE, TRUE, sizeof (GValue));
  g_array_set_size (values, query.n_params + 1);
  g_array_set_clear_func (values, (GDestroyNotify) g_value_unset);

  /* Set instance */
  instance_and_params = &g_array_index (values, GValue, 0);
  g_value_init_from_instance (instance_and_params, gobject);

  /* Set parameters */
  for (i = 0; i < query.n_params; i++)
    {
      GValue *val = &g_array_index (values, GValue, i+1);

      variant = g_variant_get_child_value (params, i+1);
      g_value_init (val, query.param_types[i]);
      value_set_variant (val, variant, clip->app);
    }

  /* Setup return value */
  if (query.return_type && query.return_type != G_TYPE_NONE)
    g_value_init (&retval, query.return_type);

  /* Emit signal */
  g_signal_emitv (instance_and_params, id, g_quark_try_string (detail), &retval);
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

  if (g_strcmp0 (method_name, "Highlight") == 0)
    {
      g_autofree gchar *object;

      g_variant_get (parameters, "(s)", &object);
      clippy_highlight (clip, object, &error);
    }
  else if (g_strcmp0 (method_name, "Clear") == 0)
    clippy_clear (clip);
  else if (g_strcmp0 (method_name, "Message") == 0)
    {
      g_autofree gchar *id, *text, *image, *relative_to;
      guint timeout;

      g_variant_get (parameters, "(ssssu)", &id, &text, &image, &relative_to, &timeout);
      clippy_message (clip, id, text, image, relative_to, timeout, &error);
    }
  if (g_strcmp0 (method_name, "MessageClear") == 0)
    {
      g_autofree gchar *id;
      g_variant_get (parameters, "(s)", &id);
      clippy_message_clear (clip, id, &error);
    }
  else if (g_strcmp0 (method_name, "Set") == 0)
    {
      g_autofree gchar *object, *property;
      GVariant *value;

      g_variant_get (parameters, "(ssv)", &object, &property, &value);
      clippy_set (clip, object, property, value, &error);

      g_variant_unref (value);
    }
  else if (g_strcmp0 (method_name, "Get") == 0)
    {
      g_autofree gchar *object, *property;

      g_variant_get (parameters, "(ss)", &object, &property);
      clippy_get (clip, object, property, &return_value, &error);
    }
  else if (g_strcmp0 (method_name, "Connect") == 0)
    {
      g_autofree gchar *object, *signal, *detail;

      g_variant_get (parameters, "(sss)", &object, &signal, &detail);
      clippy_connect (clip, object, signal, detail, &error);
    }
  else if (g_strcmp0 (method_name, "Emit") == 0)
    {
      g_autofree gchar *signal, *detail;
      g_autoptr(GVariant) params = NULL;

      g_variant_get (parameters, "(ssv)", &signal, &detail, &params);
      clippy_emit (clip, signal, detail, params, &error);
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

static gboolean
clippy_idle (gpointer user_data)
{
  GApplication *app;
  
  if ((app = g_application_get_default ()) &&
      g_application_get_is_registered (app))
    {
      const static GDBusInterfaceVTable vtable = {
        clippy_method_call,
        NULL,
        clippy_set_property
      };
      
      g_dbus_connection_register_object (g_application_get_dbus_connection (app),
                                         g_application_get_dbus_object_path (app),
                                         iface_info,
                                         &vtable,
                                         clippy_new (app),
                                         (GDestroyNotify) clippy_free,
                                         NULL);
    }

  return G_SOURCE_REMOVE;
}

__attribute__((constructor))
void clippy_init (void)
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
      
      /* Give the app a chance to initialize */
      g_idle_add (clippy_idle, NULL);
    }
  else
    g_debug ("%s %s", __func__, error->message);
}

