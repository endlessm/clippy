/* clippy-dbus-wrapper.c
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

#include <gio/gio.h>
#include "clippy-dbus-wrapper.h"
#include "utils.h"

typedef struct
{
  GObject            *object;
  gulong              notify_id;
  gchar              *object_path;
  GDBusInterfaceInfo *info;
  GDBusConnection    *connection;
} ClippyDbusWrapperPrivate;

struct _ClippyDbusWrapper
{
  GDBusInterfaceSkeleton parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClippyDbusWrapper, clippy_dbus_wrapper, G_TYPE_DBUS_INTERFACE_SKELETON)

#define CLIPPY_DBUS_WRAPPER_PRIVATE(o) clippy_dbus_wrapper_get_instance_private ((ClippyDbusWrapper *)o)

enum {
  PROP_0,
  PROP_OBJECT,
  PROP_OBJECT_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
clippy_dbus_wrapper_set_object (GObject *wrapper, GObject *object);

ClippyDbusWrapper *
clippy_dbus_wrapper_new (GObject *object, const gchar *object_path)
{
  return g_object_new (CLIPPY_TYPE_DBUS_WRAPPER,
                       "object", object,
                       "object-path", object_path,
                       NULL);
}

static void
clippy_dbus_wrapper_dispose (GObject *object)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (object);

  if (priv->object && priv->notify_id)
    {
      g_signal_handler_disconnect (priv->object, priv->notify_id);
      priv->notify_id = 0;
    }

  g_clear_object (&priv->object);

  G_OBJECT_CLASS (clippy_dbus_wrapper_parent_class)->dispose (object);
}

static void
clippy_dbus_wrapper_finalize (GObject *object)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (object);

  g_clear_pointer (&priv->object_path, g_free);
  g_clear_pointer (&priv->info, g_dbus_interface_info_unref);

  G_OBJECT_CLASS (clippy_dbus_wrapper_parent_class)->finalize (object);
}

static void
clippy_dbus_wrapper_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_OBJECT:
        g_value_set_object (value, priv->object);
      break;
      case PROP_OBJECT_PATH:
        g_value_set_string (value, priv->object_path);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clippy_dbus_wrapper_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_OBJECT:
        clippy_dbus_wrapper_set_object (object, g_value_get_object (value));
      break;
      case PROP_OBJECT_PATH:
        g_free (priv->object_path);
        priv->object_path = g_strdup (g_value_get_string (value));
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_skeleton_handle_method_call (GDBusConnection       *connection,
                              const gchar           *sender,
                              const gchar           *object_path,
                              const gchar           *interface_name,
                              const gchar           *method_name,
                              GVariant              *parameters,
                              GDBusMethodInvocation *invocation,
                              gpointer               user_data)
{
  g_message ("%s %s %s %s %s", __func__, sender, object_path, interface_name, method_name);
}

static GVariant *
_skeleton_handle_get_property (GDBusConnection *connection,
                               const gchar     *sender,
                               const gchar     *object_path,
                               const gchar     *interface_name,
                               const gchar     *property_name,
                               GError         **error,
                               gpointer         skeleton)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (skeleton);
  g_auto(GValue) gvalue = G_VALUE_INIT;
  GParamSpec *pspec;

  g_debug ("%s %s %s %s %s", __func__, sender, object_path, interface_name, property_name);

  if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (priv->object), property_name)))
    return NULL;

  g_value_init (&gvalue, pspec->value_type);
  g_object_get_property (priv->object, property_name, &gvalue);

  return variant_new_value (&gvalue);
}

static gboolean
_skeleton_handle_set_property (GDBusConnection *connection,
                               const gchar     *sender,
                               const gchar     *object_path,
                               const gchar     *interface_name,
                               const gchar     *property_name,
                               GVariant        *variant,
                               GError         **error,
                               gpointer         skeleton)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (skeleton);
  g_auto(GValue) gvalue = G_VALUE_INIT;
  GParamSpec *pspec;

  g_debug ("%s %s %s %s %s", __func__, sender, object_path, interface_name, property_name);

  if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (priv->object), property_name)))
    return FALSE;

  g_value_init (&gvalue, pspec->value_type);
  value_set_variant (&gvalue, variant);
  g_object_set_property (priv->object, property_name, &gvalue);

  return TRUE;
}

static GDBusInterfaceInfo *
clippy_dbus_wrapper_skeleton_dbus_interface_get_info (GDBusInterfaceSkeleton *skeleton)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (skeleton);
  return priv->info;
}

static GDBusInterfaceVTable *
clippy_dbus_wrapper_skeleton_dbus_interface_get_vtable (GDBusInterfaceSkeleton *skeleton)
{
  static GDBusInterfaceVTable skeleton_vtable = {
    _skeleton_handle_method_call,
    _skeleton_handle_get_property,
    _skeleton_handle_set_property,
    {NULL}
  };

  return &skeleton_vtable;
}

static GVariant *
clippy_dbus_wrapper_skeleton_dbus_interface_get_properties (GDBusInterfaceSkeleton *skeleton)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (skeleton);
  g_autoptr(GVariantBuilder) b = NULL;
  g_autofree GParamSpec **props;
  guint n_props;

  b = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

  /* List properties */
  props = g_object_class_list_properties (G_OBJECT_GET_CLASS (priv->object), &n_props);
  for (gint i = 0; i < n_props; i++)
    {
      GParamSpec *pspec = props[i];
      g_auto(GValue) value = G_VALUE_INIT;

      /* Ignore properties that are not readable or writable */
      if (!(pspec->flags & G_PARAM_READABLE) ||
          !(pspec->flags & G_PARAM_WRITABLE && !(pspec->flags & G_PARAM_CONSTRUCT_ONLY)))
        continue;

      g_value_init (&value, pspec->value_type);
      g_object_get_property (priv->object, pspec->name, &value);
      g_variant_builder_add (b, "{sv}", pspec->name, variant_new_value (&value));
    }
  return g_variant_builder_end (b);
}

static void
clippy_dbus_wrapper_skeleton_dbus_interface_flush (GDBusInterfaceSkeleton *_skeleton)
{
}

static void
clippy_dbus_wrapper_class_init (ClippyDbusWrapperClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GDBusInterfaceSkeletonClass *skeleton_class;

  object_class->finalize = clippy_dbus_wrapper_finalize;
  object_class->dispose = clippy_dbus_wrapper_dispose;
  object_class->get_property = clippy_dbus_wrapper_get_property;
  object_class->set_property = clippy_dbus_wrapper_set_property;

  skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (klass);
  skeleton_class->get_info = clippy_dbus_wrapper_skeleton_dbus_interface_get_info;
  skeleton_class->get_properties = clippy_dbus_wrapper_skeleton_dbus_interface_get_properties;
  skeleton_class->flush = clippy_dbus_wrapper_skeleton_dbus_interface_flush;
  skeleton_class->get_vtable = clippy_dbus_wrapper_skeleton_dbus_interface_get_vtable;

  properties[PROP_OBJECT] =
    g_param_spec_object ("object",
                         "Object",
                         "The wrapped object",
                         G_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_OBJECT_PATH] =
    g_param_spec_string ("object-path",
                         "Object Path",
                         "The DBus path for the wrapped object",
                         NULL,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
clippy_dbus_wrapper_init (ClippyDbusWrapper *self)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (self);

  priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  priv->info = g_new0 (GDBusInterfaceInfo, 1);
  priv->info->ref_count = 1;
}

static void
on_object_notify (GObject           *gobject,
                  GParamSpec        *pspec,
                  ClippyDbusWrapper *self)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (self);
  g_auto(GVariantBuilder) builder, invalidated_builder;
  g_auto(GValue) value = G_VALUE_INIT;
  g_autoptr(GError) error = NULL;

  if (!priv->object_path)
    return;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));

  g_value_init (&value, pspec->value_type);
  g_object_get_property (gobject, pspec->name, &value);
  g_variant_builder_add (&builder, "{sv}", pspec->name, variant_new_value (&value));
  g_variant_builder_add (&invalidated_builder, "s", pspec->name);

  g_dbus_connection_emit_signal (priv->connection,
                                 NULL,
                                 priv->object_path,
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)",
                                                CLIPPY_DBUS_WRAPPER_IFACE,
                                                &builder,
                                                &invalidated_builder),
                                 &error);
  if (error)
    g_message ("%s %s", __func__, error->message);
}

static void
clippy_dbus_wrapper_set_object (GObject *wrapper, GObject *object)
{
  ClippyDbusWrapperPrivate *priv = CLIPPY_DBUS_WRAPPER_PRIVATE (wrapper);
  GDBusInterfaceInfo *info = priv->info;
  GArray *properties;
  g_autofree GParamSpec **props;
  guint n_props;

  g_set_object (&priv->object, object);
  priv->notify_id = g_signal_connect_object (object, "notify",
                                             G_CALLBACK (on_object_notify),
                                             wrapper, 0);

  /* Generate interface information */
  info->name = CLIPPY_DBUS_WRAPPER_IFACE;

  properties = g_array_new (TRUE, TRUE, sizeof (GDBusPropertyInfo *));

  /* List properties */
  props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &n_props);
  for (gint i = 0; i < n_props; i++)
    {
      GParamSpec *pspec = props[i];
      GDBusPropertyInfo *property;

      /* Ignore properties that are not readable or writable */
      if (!(pspec->flags & G_PARAM_READABLE) ||
          !(pspec->flags & G_PARAM_WRITABLE && !(pspec->flags & G_PARAM_CONSTRUCT_ONLY)))
        continue;

      property = g_slice_new0 (GDBusPropertyInfo);
      property->name = g_strdup (pspec->name);
      property->signature = (gchar *) signature_from_type (pspec->value_type);

      property->flags = G_DBUS_PROPERTY_INFO_FLAGS_NONE;

      if (pspec->flags & G_PARAM_READABLE)
        property->flags |= G_DBUS_PROPERTY_INFO_FLAGS_READABLE;
      if (pspec->flags & G_PARAM_WRITABLE && !(pspec->flags & G_PARAM_CONSTRUCT_ONLY))
        property->flags |= G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE;

      g_array_append_val (properties, property);
    }

  info->properties = (GDBusPropertyInfo **) g_array_free (properties, FALSE);
}

