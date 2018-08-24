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

  GClosure       *signal_closure;
  GClosure       *notify_closure;

  GVariant       *parameters;
  
  gchar          *css;
} Clippy;

static inline void
clippy_emit_signal (Clippy      *clip,
                    const gchar *signal_name,
                    GVariant    *parameters,
                    GError     **error)
{
  g_dbus_connection_emit_signal (g_application_get_dbus_connection (clip->app),
                                 NULL,
                                 g_application_get_dbus_object_path (clip->app),
                                 DBUS_IFACE,
                                 signal_name,
                                 parameters,
                                 error);
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
  GObject *object = NULL;
  GVariantBuilder builder;
  GVariant *params = NULL;
  const gchar *id;
  
  if (!n_param_values || !G_VALUE_HOLDS_OBJECT (param_values))
    return;
  
  object = g_value_get_object (param_values);
  id = object_get_name (object);

  g_debug ("%s %s %s %s %d", __func__,
           G_OBJECT_TYPE_NAME (object), 
           id,
           g_signal_name (hint->signal_id),
           n_param_values);

  if (n_param_values > 1)
    {
      gint i;
      
      g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
      
      /* Add extra parameters */
      for (i = 1; i < n_param_values; i++)
        {
          GVariant *variant = variant_new_value (&param_values[i]);
          
          if (!variant)  
            variant = g_variant_new_maybe (G_VARIANT_TYPE_STRING, NULL);

          g_variant_builder_add_value (&builder, variant);
        }

      params = g_variant_new_variant (g_variant_builder_end (&builder));
    }

  /* Create signal parameters */
  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  g_variant_builder_add_value (&builder, g_variant_new_string (id));
  g_variant_builder_add_value (&builder, g_variant_new_string (g_signal_name (hint->signal_id)));
  g_variant_builder_add_value (&builder, g_variant_new_string (hint->detail ? g_quark_to_string (hint->detail) : ""));
  if (params)
    g_variant_builder_add_value (&builder, params);
  
  /* Emit D-Bus signal */
  clippy_emit_signal (clip, "ObjectSignal",
                      g_variant_builder_end (&builder),
                      NULL);
}

static void
notify_closure_callback (GObject    *gobject,
                         GParamSpec *pspec,
                         Clippy    *clip)
{
  GValue value = G_VALUE_INIT;
  GVariantBuilder builder;
  GVariant *variant;
  const gchar *id;

  id = object_get_name (gobject);
  g_debug ("%s %s %s", __func__, id, pspec->name);
  
  g_value_init (&value, pspec->value_type);
  g_object_get_property (gobject, pspec->name, &value);
  variant = variant_new_value (&value);

  /* Create signal parameters */
  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  g_variant_builder_add_value (&builder, g_variant_new_string (id));
  g_variant_builder_add_value (&builder, g_variant_new_string (pspec->name));
  g_variant_builder_add_value (&builder, g_variant_new_variant (variant));

  /* Emit D-Bus signal */
  clippy_emit_signal (clip, "ObjectNotify",
                      g_variant_builder_end (&builder),
                      NULL);

  g_value_unset (&value);
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
clippy_free (Clippy *data)
{
  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (data->provider));
  g_clear_object (&data->provider);
  g_clear_pointer (&data->signal_closure, g_closure_unref);
  g_clear_pointer (&data->notify_closure, g_closure_unref);
  g_clear_pointer (&data->css, g_free);
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
clippy_set (Clippy       *clip,
            const gchar  *object,
            const gchar  *property,
            GVariant     *variant,
            GError      **error)
{
  GValue gvalue = G_VALUE_INIT;
  GObject *gobject;
  GParamSpec *pspec;
  
  g_debug ("%s %s %s", __func__, object, property);
  
  if (app_get_object_info (clip->app, object, property, NULL,
                           &gobject, &pspec, NULL, error))
    return;

  g_value_init (&gvalue, pspec->value_type);
  value_set_variant (&gvalue, variant);
  
  g_object_set_property (gobject, property, &gvalue);
  
  g_value_unset (&gvalue);
}

static void
clippy_get (Clippy       *clip,
            const gchar  *object,
            const gchar  *property,
            GVariant    **variant,
            GError      **error)
{
  GValue gvalue = G_VALUE_INIT;
  GObject *gobject;
  GParamSpec *pspec;
  
  g_debug ("%s %s %s", __func__, object, property);

  if (app_get_object_info (clip->app, object, property, NULL,
                           &gobject, &pspec, NULL, error))
    return;

  g_value_init (&gvalue, pspec->value_type);
  g_object_get_property (gobject, property, &gvalue);
  
  *variant = variant_new_value (&gvalue);

  g_value_unset (&gvalue);
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
value_init_array_from_variant (GValue      *values,
                               guint        n_params,
                               const GType *param_types,
                               GVariant    *params)
{
  gint i;

  if (!n_params || !params)
    return;
  
  for (i = 0; i < n_params; i++)
    {
      g_autoptr(GVariant) variant = g_variant_get_child_value (params, i);

      g_value_init (&values[i], param_types[i]);
      value_set_variant (&values[i], variant);
    }
}

static void
clippy_emit (Clippy       *clip,
             const gchar  *object,
             const gchar  *signal,
             const gchar  *detail,
             GVariant     *params,
             GError      **error)
{
  GValue retval = G_VALUE_INIT;
  GValue *instance_and_params;
  GObject *gobject;
  GSignalQuery query;
  guint id, i;

  if (app_get_object_info (clip->app, object, NULL, signal,
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

  if (query.return_type && query.return_type != G_TYPE_NONE)
    g_value_init (&retval, query.return_type);
  
  instance_and_params = g_new0 (GValue, query.n_params + 1);

  /* Set object */
  g_value_init (instance_and_params, G_TYPE_OBJECT);
  g_value_set_object (instance_and_params, gobject);

  /* Set paramenters */
  value_init_array_from_variant (&instance_and_params[1],
                                 query.n_params,
                                 query.param_types,
                                 params);

  /* Emit signal */
  g_signal_emitv (instance_and_params, id, g_quark_try_string (detail), &retval);

  /* Unset params */
  for (i = 0; i <= query.n_params; i++)
    g_value_unset (&instance_and_params[i]);

  if (query.return_type && query.return_type != G_TYPE_NONE)
    g_value_unset (&retval);

  g_free (instance_and_params);
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
      g_autoptr(GVariant) value = NULL;

      g_variant_get (parameters, "(ss)", &object, &property);
      clippy_get (clip, object, property, &value, &error);
      
      if (!error && value)
        {
          GVariantBuilder builder;
          g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
          g_variant_builder_add_value (&builder, g_variant_new_variant (value));
          return_value = g_variant_builder_end (&builder);
        }
    }
  else if (g_strcmp0 (method_name, "Connect") == 0)
    {
      g_autofree gchar *object, *signal, *detail;

      g_variant_get (parameters, "(sss)", &object, &signal, &detail);
      clippy_connect (clip, object, signal, detail, &error);
    }
  else if (g_strcmp0 (method_name, "Emit") == 0)
    {
      g_autofree gchar *object, *signal, *detail;
      g_autoptr(GVariant) params = NULL;

      g_variant_get (parameters, "(sssv)", &object, &signal, &detail, &params);
      clippy_emit (clip, object, signal, detail, params, &error);
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
      
      /* Give the app a change to initialize */
      g_idle_add (clippy_idle, NULL);
    }
  else
    g_debug ("%s %s", __func__, error->message);
}

