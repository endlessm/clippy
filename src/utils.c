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

/*
 * Get widget name first or buildable name instead
 */
const gchar *
object_get_name (GObject *object)
{
  const gchar *name;

  if (!object)
    return NULL;
  
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

GObject *
app_get_object (GApplication *app, const gchar *name)
{
  FindData data = { name, NULL };
  gboolean const_toplevels;
  GList *toplevels, *l;
  
  if (app && (const_toplevels = GTK_IS_APPLICATION (app)))
    toplevels = gtk_application_get_windows (GTK_APPLICATION (app));
  else
    toplevels = gdk_screen_get_toplevel_windows (gdk_screen_get_default ());

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

  return data.object;
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
    return g_variant_new_double (g_value_get_boolean (value));
  else if (type == G_TYPE_STRING)
    {
      const gchar *string = g_value_get_string (value);
      return g_variant_new_string (string ? string : "");
    }
  else if (type == G_TYPE_OBJECT)
    return g_variant_new_string (object_get_name (g_value_get_object (value)));
  else if (type == G_TYPE_GTYPE)
    return g_variant_new_string (g_type_name (g_value_get_gtype (value)));
  else if (type == G_TYPE_VARIANT)
    return g_value_dup_variant (value);
  /* Boxed types */
  else if (type == G_TYPE_GSTRING)
    {
      GString *string = g_value_get_boxed (value);
      return g_variant_new_string ((string && string->str) ? string->str: "");
    }
  else if (type == G_TYPE_STRV)
    {
      const gchar **strv = g_value_get_boxed (value);
      return g_variant_new_strv (strv, -1);
    }

  return NULL;
}

gboolean
value_set_variant (GValue *value, GVariant *variant)
{
  GType type = G_VALUE_TYPE (value);
  GValue gvalue = G_VALUE_INIT;

  if (type == G_TYPE_GTYPE)
    g_value_set_gtype (value, g_type_from_name (g_variant_get_string (variant, NULL)));
  else
    {
      g_dbus_gvariant_to_gvalue (variant, &gvalue);
      g_value_transform (&gvalue, value);
      g_value_unset (&gvalue);
    }

  return TRUE;
}

