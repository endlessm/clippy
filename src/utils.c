/* utils.c
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

#include "utils.h"
#include "clippy-js-proxy.h"
#include "webkit-marshal.h"

G_DEFINE_QUARK(clippy_error, clippy)

/*
 * Get widget name first or buildable name instead
 */
const gchar *
object_get_name (GObject *object)
{
  const gchar *name;

  if (!object)
    return NULL;

  if (CLIPPY_IS_JS_PROXY (object))
    return g_object_get_data (object, "__Clippy_object_name_");
  
  if (GTK_IS_WIDGET (object) &&
      (name = gtk_widget_get_name ((GtkWidget *)object)) &&
      g_strcmp0 (name, G_OBJECT_TYPE_NAME (object)))
    return name;

  if (GTK_IS_BUILDABLE (object) &&
      (name = gtk_buildable_get_name (GTK_BUILDABLE (object))) &&
      !g_str_has_prefix (name, "___object_"))
    return name;

  return NULL;
}

typedef struct
{
  const gchar *name;
  GObject *object;
} FindData;

static void
find_object_forall (GtkWidget *widget, gpointer user_data)
{
  FindData *data = user_data;

  if (data->object)
    return;
  
  if (g_strcmp0 (object_get_name ((GObject *) widget), data->name) == 0 &&
      gtk_widget_is_visible (widget))
    {
      data->object = (GObject *) widget;
      return;
    }
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall ((GtkContainer *) widget, find_object_forall, data);
}

static inline GObject *
app_get_object (GApplication *app, const gchar *name, GError **error)
{
  gboolean const_toplevels = FALSE;
  g_auto(GStrv) tokens = NULL;
  FindData data = { NULL, };
  GList *toplevels, *l;

  if (!name)
    return NULL;

  if (app && (const_toplevels = GTK_IS_APPLICATION (app)))
    toplevels = gtk_application_get_windows (GTK_APPLICATION (app));
  else
    toplevels = gdk_screen_get_toplevel_windows (gdk_screen_get_default ());

  /* name can have dot property access operator */
  tokens = g_strsplit (name, ".", -1);
  data.name = tokens[0];

  for (l = toplevels; l; l = g_list_next (l))
    {
      if (!gtk_widget_is_visible (l->data))
        continue;
      
      gtk_container_forall (l->data, find_object_forall, &data);
      
      if (data.object)
        break;
    }

  if (!const_toplevels)
    g_list_free (toplevels);

  clippy_return_val_if_fail (data.object,
                             NULL, error, CLIPPY_NO_OBJECT,
                             "Object '%s' not found",
                             tokens[0]);

  if (tokens[1])
    {
      GType webview_type = webkit_web_view_get_type ();
      GObject *objval = data.object;
      gint i;

      for (i = 1; tokens[i]; i++)
        {
          GParamSpec *pspec;

          pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (objval),
                                                tokens[i]);

          /*
           * Check if we are trying to access a JS object
           */
          if (pspec == NULL && webview_type &&
              g_strcmp0 (tokens[i], "JSContext") == 0 &&
              G_TYPE_CHECK_INSTANCE_TYPE ((objval), webview_type))
            {
              const gchar *js_object_name = tokens[i+1];
              g_autofree gchar *key = NULL;
              GObject *js_object;

              clippy_return_val_if_fail (js_object_name != NULL,
                                         NULL, error, CLIPPY_NO_OBJECT,
                                         "Need to specify a JSContext object name for '%s'",
                                         tokens[i-1]);

              key = g_strconcat ("__Clippy_JSContext_", js_object_name, NULL);

              if ((js_object = g_object_get_data (objval, key)))
                  return js_object;

              js_object = clippy_js_proxy_new (objval, js_object_name);
              g_object_set_data_full (js_object,
                                      "__Clippy_object_name_",
                                      g_strdup (name),
                                      g_free);
              g_object_set_data_full (objval,
                                      key,
                                      js_object,
                                      g_object_unref);

              return js_object;
            }

          /* Check the property exists, is readable and object type */
          clippy_return_val_if_fail (pspec,
                                     NULL, error, CLIPPY_NO_OBJECT,
                                     "Object '%s' has no property '%s'",
                                     tokens[i-1],
                                     tokens[i]);

          clippy_return_val_if_fail ((pspec->flags & G_PARAM_READABLE),
                                     NULL, error, CLIPPY_NO_OBJECT,
                                     "Can not read property '%s' from object '%s'",
                                     tokens[i],
                                     tokens[i-1]);

          clippy_return_val_if_fail (g_type_is_a (pspec->value_type, G_TYPE_OBJECT),
                                     NULL, error, CLIPPY_NO_OBJECT,
                                     "Property '%s' from object '%s' is not an object type",
                                     tokens[i],
                                     tokens[i-1]);

          g_object_get (objval, tokens[i], &objval, NULL);

          clippy_return_val_if_fail (objval,
                                     NULL, error, CLIPPY_NO_OBJECT,
                                     "Object '%s.%s' property is not set",
                                     tokens[i-1],
                                     tokens[i]);
          g_object_unref (objval);
        }

      return objval;
    }

  return data.object;
}

gboolean
app_get_object_info (GApplication *app,
                     const gchar  *object,
                     const gchar  *property,
                     const gchar  *signal,
                     GObject     **gobject,
                     GParamSpec  **pspec,
                     guint        *signal_id,
                     GError      **error)
{

  if (!(app && object))
    {
      if (error)
        *error = g_error_new_literal (CLIPPY_ERROR,
                                      CLIPPY_UNKNOWN_ERROR,
                                      "Unknown error");
      return TRUE;
    }

  if (!gobject)
    return FALSE;

  if (!(*gobject = app_get_object (app, object, error)))
    return TRUE;

  if (property && pspec)
    clippy_return_val_if_fail (*pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (*gobject),
                                                                      property),
                               TRUE, error, CLIPPY_NO_PROPERTY,
                               "No property '%s' found on object '%s'",
                               property,
                               object);

  if (signal && signal_id)
    clippy_return_val_if_fail (*signal_id = g_signal_lookup (signal, G_OBJECT_TYPE (*gobject)),
                               TRUE, error, CLIPPY_NO_SIGNAL,
                               "Object '%s' of type %s has no signal '%s'",
                               object,
                               G_OBJECT_TYPE_NAME (*gobject),
                               signal);
  return FALSE;
}

GVariant *
variant_new_value (const GValue *value)
{
  GType type = G_VALUE_TYPE (value);

  if (type == G_TYPE_CHAR)
    return g_variant_new_byte (g_value_get_schar (value));
  else if (type == G_TYPE_UCHAR)
    return g_variant_new_byte (g_value_get_uchar (value));
  else if (type == G_TYPE_BOOLEAN)
    return g_variant_new_boolean (g_value_get_boolean (value));
  else if (type == G_TYPE_INT)
    return g_variant_new_int64 (g_value_get_int (value));
  else if (type == G_TYPE_UINT)
    return g_variant_new_uint64 (g_value_get_uint (value));
  else if (type == G_TYPE_LONG)
    return g_variant_new_int64 (g_value_get_long (value));
  else if (type == G_TYPE_ULONG)
    return g_variant_new_uint64 (g_value_get_ulong (value));
  else if (type == G_TYPE_INT64)
    return g_variant_new_int64 (g_value_get_int64 (value));
  else if (type == G_TYPE_UINT64)
    return g_variant_new_uint64 (g_value_get_uint64 (value));
  else if (type == G_TYPE_ENUM)
    return g_variant_new_int64 (g_value_get_enum (value));
  else if (type == G_TYPE_FLAGS)
    return g_variant_new_uint64 (g_value_get_flags (value));
  else if (type == G_TYPE_FLOAT)
    return g_variant_new_double (g_value_get_float (value));
  else if (type == G_TYPE_DOUBLE)
    return g_variant_new_double (g_value_get_double (value));
  else if (type == G_TYPE_STRING)
    {
      const gchar *string = g_value_get_string (value);
      return g_variant_new_string (string ? string : "");
    }
  else if (type == G_TYPE_GTYPE)
    return g_variant_new_string (g_type_name (g_value_get_gtype (value)));
  else if (type == G_TYPE_VARIANT)
    return g_value_dup_variant (value);
  /* Boxed types */
  else if (type == G_TYPE_GSTRING)
    {
      GString *string = g_value_get_boxed (value);
      return g_variant_new_string ((string && string->str) ? string->str : "");
    }
  else if (type == G_TYPE_STRV)
    {
      const gchar **strv = g_value_get_boxed (value);
      return g_variant_new_strv (strv, -1);
    }
  else if (type == G_TYPE_OBJECT || g_type_is_a (type, G_TYPE_OBJECT))
    {
      const gchar *id = object_get_name (g_value_get_object (value));
      return g_variant_new_string (id ? id : "");
    }

  return g_variant_new_string ("");
}

gboolean
value_set_variant (GValue *value, GVariant *variant, GApplication *app)
{
  GType type = G_VALUE_TYPE (value);

  if (type == G_TYPE_GTYPE)
    g_value_set_gtype (value, g_type_from_name (g_variant_get_string (variant, NULL)));
  else if (app && (type == G_TYPE_OBJECT || g_type_is_a (type, G_TYPE_OBJECT)))
    {
      const gchar *object = g_variant_get_string (variant, NULL);
      g_value_set_object (value, app_get_object (app, object, NULL));
    }
  else
    {
      g_auto(GValue) gvalue = G_VALUE_INIT;
      g_dbus_gvariant_to_gvalue (variant, &gvalue);
      g_value_transform (&gvalue, value);
    }

  return TRUE;
}

void
str_replace_char (gchar *str, gchar a, gchar b)
{
  g_return_if_fail (str != NULL);

  while (*str != 0)
    {
      if (*str == a)
        *str = b;

      str = g_utf8_next_char (str);
    }
}


