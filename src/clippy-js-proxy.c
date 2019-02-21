/* clippy-js-proxy.c
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

#include "clippy-js-proxy.h"
#include "webkit-marshal.h"
#include "utils.h"

typedef struct
{
  gchar          *object_name;
  WebKitWebView  *webview;
} ClippyJsProxyPrivate;

struct ClippyJsProxy
{
  GObject parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClippyJsProxy, clippy_js_proxy, G_TYPE_OBJECT)

#define CLIPPY_JS_PROXY_PRIVATE(o) clippy_js_proxy_get_instance_private ((ClippyJsProxy *) o)

enum {
  PROP_0,
  PROP_WEBVIEW,
  PROP_OBJECT_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

typedef struct
{
  WebKitJavascriptResult *result;
  gboolean finished;
} JsRunData;

static void
_js_finish_handler (GObject *object, GAsyncResult *result, gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  JsRunData *data = user_data;

  data->result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object),
                                                     result, &error);
  data->finished = TRUE;

  if (!data->result && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Error running javascript: %s", error->message);
}

static gboolean
_js_run_printf (WebKitWebView           *webview,
                GCancellable            *cancellable,
                WebKitJavascriptResult **result,
                const gchar             *format,
                ...)
{
  JsRunData data = { NULL, FALSE };
  g_autofree gchar *script = NULL;
  va_list args;

  va_start (args, format);
  script = g_strdup_vprintf (format, args);
  va_end (args);

  if (cancellable)
    {
      webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (webview),
                                      script, cancellable,
                                      NULL, NULL);
      return FALSE;
    }

  webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (webview),
                                  script,
                                  NULL,
                                  _js_finish_handler,
                                  &data);

  /* Wait for JS to finish */
  while (!data.finished)
    g_main_context_iteration (NULL, TRUE);

  if (result)
    *result = data.result;
  else if (data.result)
    webkit_javascript_result_unref (data.result);

  return data.result == NULL;
}

static void
clippy_js_proxy_dispose (GObject *object)
{
  ClippyJsProxyPrivate *priv = CLIPPY_JS_PROXY_PRIVATE (object);

  g_clear_object (&priv->webview);

  G_OBJECT_CLASS (clippy_js_proxy_parent_class)->dispose (object);
}

static void
clippy_js_proxy_finalize (GObject *object)
{
  ClippyJsProxyPrivate *priv = CLIPPY_JS_PROXY_PRIVATE (object);

  g_clear_pointer (&priv->object_name, g_free);

  G_OBJECT_CLASS (clippy_js_proxy_parent_class)->finalize (object);
}

static void
clippy_js_proxy_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ClippyJsProxyPrivate *priv = CLIPPY_JS_PROXY_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_WEBVIEW:
        g_value_set_object (value, priv->webview);
        break;
      case PROP_OBJECT_NAME:
        g_value_set_string (value, priv->object_name);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clippy_js_proxy_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ClippyJsProxyPrivate *priv = CLIPPY_JS_PROXY_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_WEBVIEW:
        priv->webview = g_value_dup_object (value);
        break;
      case PROP_OBJECT_NAME:
        priv->object_name = g_value_dup_string (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
clippy_js_proxy_class_init (ClippyJsProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = clippy_js_proxy_dispose;
  object_class->finalize     = clippy_js_proxy_finalize;
  object_class->get_property = clippy_js_proxy_get_property;
  object_class->set_property = clippy_js_proxy_set_property;

  properties[PROP_WEBVIEW] =
    g_param_spec_object ("webview",
                         "Webview",
                         "The WebKit webview to get the JS context",
                         WEBKIT_TYPE_WEB_VIEW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_OBJECT_NAME] =
    g_param_spec_string ("object-name",
                         "Object name",
                         "The global javascript object name to proxy properties from",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
clippy_js_proxy_init (ClippyJsProxy *self)
{
}

/* Derived Class */

typedef struct
{
  ClippyJsProxyClass   parent_class;
  GParamSpec         **properties;
  guint                n_props;
} ClippyJsProxyDerivedClass;

typedef struct
{
  GObject        parent_instance;
  GValue        *values;
  GCancellable **cancellable;
} ClippyJsProxyDerived;

#define CLIPPY_JS_PROXY_DERIVED(o) (G_TYPE_CHECK_INSTANCE_CAST ((o),  G_OBJECT_TYPE(o), ClippyJsProxyDerived))
#define CLIPPY_JS_PROXY_DERIVED_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), G_OBJECT_TYPE(o), ClippyJsProxyDerivedClass))
#define CLIPPY_JS_PROXY_DERIVED_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), G_OBJECT_TYPE(o), ClippyJsProxyDerivedClass))

static void
clippy_js_proxy_derived_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ClippyJsProxyDerivedClass *klass = CLIPPY_JS_PROXY_DERIVED_GET_CLASS (object);

  if (0 < prop_id && prop_id < klass->n_props)
    {
      ClippyJsProxyDerived *proxy = CLIPPY_JS_PROXY_DERIVED (object);
      g_value_copy (&proxy->values[prop_id], value);
      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
clippy_js_proxy_derived_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ClippyJsProxyDerivedClass *klass = CLIPPY_JS_PROXY_DERIVED_GET_CLASS (object);

  if (0 < prop_id && prop_id < klass->n_props)
    {
      ClippyJsProxyPrivate *priv = CLIPPY_JS_PROXY_PRIVATE (object);
      ClippyJsProxyDerived *proxy = CLIPPY_JS_PROXY_DERIVED (object);
      g_autofree gchar *prop = NULL;
      GCancellable *cancellable;

      if (g_param_values_cmp (pspec, value, &proxy->values[prop_id]) == 0)
        return;

      g_value_copy (value, &proxy->values[prop_id]);

      /* Do we need to wait for the value to be updated in JS? */
      g_object_notify_by_pspec (object, pspec);

      if (!priv->webview)
        return;

      /* Cancel previous set operation */
      g_cancellable_cancel (proxy->cancellable[prop_id]);
      g_clear_object (&proxy->cancellable[prop_id]);

      proxy->cancellable[prop_id] = cancellable = g_cancellable_new ();

      prop = g_strdup (pspec->name);
      str_replace_char (prop, '-', '_');

      /* FIXME: passing a cancellable makes it crash when calling from DBus */
      if (pspec->value_type == G_TYPE_BOOLEAN)
        {
          _js_run_printf (priv->webview, NULL, NULL, "%s.%s=%s;",
                          priv->object_name, prop,
                          g_value_get_boolean (value) ? "true" : "false");
        }
      else if (pspec->value_type == G_TYPE_STRING)
        {
          g_autofree gchar *escaped = g_strescape (g_value_get_string (value),
                                                   NULL);
          _js_run_printf (priv->webview, NULL, NULL, "%s.%s=\"%s\";",
                          priv->object_name, prop, escaped);
        }
      else if (pspec->value_type == G_TYPE_DOUBLE)
        {
          _js_run_printf (priv->webview, NULL, NULL, "%s.%s=%lf;",
                          priv->object_name, prop,
                          g_value_get_double (value));
        }

      return;
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
clippy_js_proxy_derived_finalize (GObject *object)
{
  ClippyJsProxyDerivedClass *klass = CLIPPY_JS_PROXY_DERIVED_GET_CLASS (object);
  ClippyJsProxyDerived *proxy = CLIPPY_JS_PROXY_DERIVED (object);

  /* Unset values */
  for (gint i = 1; i < klass->n_props; i++)
    {
      g_value_unset (&proxy->values[i]);

      g_cancellable_cancel (proxy->cancellable[i]);
      g_clear_object (&proxy->cancellable[i]);
    }

  g_clear_pointer (&proxy->values, g_free);
  g_clear_pointer (&proxy->cancellable, g_free);

  /* Chain up */
  clippy_js_proxy_finalize (object);
}

static void
clippy_js_proxy_derived_init (ClippyJsProxy *self)
{
  ClippyJsProxyDerivedClass *klass = CLIPPY_JS_PROXY_DERIVED_GET_CLASS (self);
  ClippyJsProxyDerived *proxy = CLIPPY_JS_PROXY_DERIVED (self);

  /* Allocate values array */
  proxy->values = g_new0 (GValue, klass->n_props);

  /* Allocate cancellable slots for each property */
  proxy->cancellable = g_new0 (GCancellable *, klass->n_props);

  /* Initialize values */
  for (gint i = 1; i < klass->n_props; i++)
    {
      GParamSpec *pspec = klass->properties[i];
      GValue *value = &proxy->values[i];

      g_value_init (value, pspec->value_type);

      /* Set default value */
      g_value_copy (g_param_spec_get_default_value (pspec), value);
    }
}

static void
js_value_to_value (JSCValue *src, GValue *dest)
{
  if (jsc_value_is_boolean (src))
    {
      if (!G_VALUE_TYPE (dest))
        g_value_init (dest, G_TYPE_BOOLEAN);
      g_value_set_boolean (dest, jsc_value_to_boolean (src));
    }
  else if (jsc_value_is_string (src))
    {
      if (!G_VALUE_TYPE (dest))
        g_value_init (dest, G_TYPE_STRING);
      g_value_take_string (dest, jsc_value_to_string (src));
    }
  else if (jsc_value_is_number (src))
    {
      if (!G_VALUE_TYPE (dest))
        g_value_init (dest, G_TYPE_DOUBLE);
      g_value_set_double (dest, jsc_value_to_double (src));
    }
  else
    g_warning ("%s unexpected GValue type", __func__);
}

JSCValue *
js_value_object_get_property (JSCValue *value, const gchar *property)
{
  g_autofree gchar *jsprop = g_strdup (property);
  str_replace_char (jsprop, '-', '_');
  return jsc_value_object_get_property (value, jsprop);
}

static void
handle_script_message_clippy_notify (WebKitUserContentManager *manager,
                                     WebKitJavascriptResult   *result,
                                     GObject                  *object)
{
  ClippyJsProxyDerivedClass *klass = CLIPPY_JS_PROXY_DERIVED_GET_CLASS (object);
  ClippyJsProxyDerived *proxy = CLIPPY_JS_PROXY_DERIVED (object);
  JSCValue *jsobject = webkit_javascript_result_get_js_value (result);
  GParamSpec **properties = klass->properties;
  g_autoptr(JSCValue) property = NULL, value = NULL;
  g_autofree char *property_str = NULL;

  property = jsc_value_object_get_property (jsobject, "property");
  value = jsc_value_object_get_property (jsobject, "value");
  property_str = jsc_value_to_string (property);

  /* Replace underscore with dash */
  str_replace_char (property_str, '_', '-');

  /* Find property, update its value and emit notify */
  for (gint i = 1; i < klass->n_props; i++)
    {
      GParamSpec *pspec = properties[i];
      if (g_strcmp0 (pspec->name, property_str) == 0)
        {
          js_value_to_value (value, &proxy->values[i]);
          g_object_notify_by_pspec (object, pspec);
          break;
        }
    }

}

static void
clippy_js_proxy_derived_sync (ClippyJsProxyDerived *object, JSCValue *jsobject)
{
  ClippyJsProxyDerivedClass *klass = CLIPPY_JS_PROXY_DERIVED_GET_CLASS (object);
  GParamSpec **properties = klass->properties;

  if (!jsc_value_is_object (jsobject))
    {
      g_warning ("Error running javascript: unexpected return value");
      return;
    }

  for (gint i = 1; properties[i]; i++)
    {
      GParamSpec *pspec = properties[i];
      g_autoptr(JSCValue) val = NULL;

      val = js_value_object_get_property (jsobject, pspec->name);
      js_value_to_value (val, &object->values[i]);
    }
}

static void
clippy_js_proxy_derived_constructed (GObject *object)
{
  ClippyJsProxyPrivate *priv = CLIPPY_JS_PROXY_PRIVATE (object);
  g_autoptr(WebKitJavascriptResult) result = NULL;
  static const gchar *clippify = NULL;

  if (!priv->webview || !priv->object_name)
    return;

  if (!clippify)
    {
      GBytes *bytes = g_resources_lookup_data ("/com/endlessm/clippy/clippify.js", 0, NULL);
      clippify = g_bytes_get_data (bytes, NULL);

      if (!clippify)
        {
          g_bytes_unref (bytes);
          return;
        }
    }

  if (!g_signal_handler_find (priv->webview,
                              G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
                              0, 0, NULL, handle_script_message_clippy_notify,
                              object))
    {
      WebKitUserContentManager *content_manager =
        webkit_web_view_get_user_content_manager (priv->webview);

      /* Setup notify mechanims */
      g_signal_connect_object (content_manager, "script-message-received::clippy_notify",
                               G_CALLBACK (handle_script_message_clippy_notify),
                               object, 0);
      webkit_user_content_manager_register_script_message_handler (content_manager,
                                                                   "clippy_notify");
    }

  if(_js_run_printf (priv->webview, NULL, &result, clippify, priv->object_name))
    return;

  clippy_js_proxy_derived_sync (CLIPPY_JS_PROXY_DERIVED (object),
                                webkit_javascript_result_get_js_value (result));
}

static void
clippy_js_proxy_derived_class_init (ClippyJsProxyDerivedClass *klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  gint n_props = 1;

  object_class->finalize = clippy_js_proxy_derived_finalize;
  object_class->get_property = clippy_js_proxy_derived_get_property;
  object_class->set_property = clippy_js_proxy_derived_set_property;
  object_class->constructed  = clippy_js_proxy_derived_constructed;

  if (!data)
    return;

  /* Save properties for our class */
  klass->properties = data;

  /* Count how many properties we have */
  while (klass->properties[n_props])
    n_props++;

  klass->n_props = n_props;

  /* Finally, Install properties */
  g_object_class_install_properties (object_class, n_props, klass->properties);
}

static GParamSpec *
param_spec_new_from_jsc (const gchar *name, JSCValue *value)
{
  GParamSpec *pspec = NULL;

  if (jsc_value_is_string (value))
    {
      g_autofree gchar *string = jsc_value_to_string (value);
      pspec = g_param_spec_string (name, "", "",
                                   string,
                                   G_PARAM_READWRITE);
    }
  else if (jsc_value_is_boolean (value))
    {
      pspec = g_param_spec_boolean (name, "", "",
                                    jsc_value_to_boolean (value),
                                    G_PARAM_READWRITE);
    }
  else if (jsc_value_is_number (value))
    {
      pspec = g_param_spec_double (name, "", "",
                                   -G_MAXDOUBLE, G_MAXDOUBLE,
                                   jsc_value_to_double (value),
                                   G_PARAM_READWRITE);
    }

  return pspec;
}

/* Public API */

/**
 * clippy_js_proxy_properties_from_js:
 * @webview: A WebKit web view
 * @object: the object to get properties from
 *
 * Get the list of properties from a JavaScript object
 *
 * Returns: a new array of GParamSpec structs
 */
GParamSpec **
clippy_js_proxy_properties_from_js (GObject *webview, const gchar *object)
{
  g_autoptr(WebKitJavascriptResult) result = NULL;
  g_auto(GStrv) properties = NULL;
  GParamSpec *pspec = NULL;
  JSCValue *value = NULL;
  GArray *array;

  if (webkit_marshall_init ())
    return NULL;

  if (_js_run_printf (webview, NULL, &result, "%s", object) || !result ||
      !(value = webkit_javascript_result_get_js_value (result)) ||
      !jsc_value_is_object (value))
    {
      g_warning ("Error running javascript: unexpected return value");
      return NULL;
    }

  array = g_array_new (FALSE, FALSE, sizeof (GParamSpec *));

  /* First property is reserved for GObject */
  g_array_append_val (array, pspec);

  /* Get all JS properties */
  properties = jsc_value_object_enumerate_properties (value);

  for (gint i = 0; properties && properties[i]; i++)
    {
      gchar *property = properties[i];

      /* Ignore private members */
      if (property[0] == '_')
        continue;

      g_autoptr(JSCValue) val = js_value_object_get_property (value, property);

      pspec = param_spec_new_from_jsc (property, val);
      g_array_append_val (array, pspec);
    }

  /* Make it null terminated */
  pspec = NULL;
  g_array_append_val (array, pspec);

  return (GParamSpec **) g_array_free (array, FALSE);
}

/**
 * clippy_js_proxy_class_new:
 * @name: the object class name
 * @properties: Null terminated array of parameter specs
 *
 * This function creates a GObject class named @name with properties defined by
 * @properties param specs array.
 * Keep in mind the first property is reserved for GObject
 *
 * Returns: a new GType
 */
GType
clippy_js_proxy_class_new (const gchar *name, GParamSpec **properties)
{
  GTypeInfo *type_info;

  g_return_val_if_fail (name != NULL, 0);

  if (webkit_marshall_init ())
    return G_TYPE_INVALID;

  type_info = g_new0 (GTypeInfo, 1);
  type_info->class_size = sizeof (ClippyJsProxyDerivedClass);
  type_info->class_init = (GClassInitFunc) clippy_js_proxy_derived_class_init;
  type_info->class_data = properties;

  type_info->instance_size = sizeof (ClippyJsProxyDerived);
  type_info->instance_init = (GInstanceInitFunc) clippy_js_proxy_derived_init;

  return g_type_register_static (CLIPPY_TYPE_JS_PROXY, name, type_info, 0);
}

/*
 * Returns a proper class name by appending CamelCase @object to "ClippyJSProxy"
 * So for example if object is my_object the returned string will be
 * "ClippyJSProxyMyObject"
 */
static inline gchar *
get_class_name_from_object (const gchar *object)
{
  g_auto(GStrv) tokens = g_strsplit (object, "_", 0);
  GString *retval = g_string_new ("ClippyJSProxy");

  for (gint i = 0; tokens[i]; i++)
    {
      const gchar *token = tokens[i];

      if (token[0] != '\0')
        g_string_append_printf (retval, "%c%s",
                                g_ascii_toupper (token[0]),
                                &token[1]);
    }

  return g_string_free (retval, FALSE);
}

/**
 * clippy_js_proxy_new:
 * @webview: a WebKit web view object
 * @object: global JS object
 *
 * This function will create a GObject proxy object for any JavaScript object
 * found in the global scope of @webview
 *
 * Returns: a new proxy object
 */
GObject *
clippy_js_proxy_new (GObject *webview, const gchar *object)
{
  g_autofree gchar *class_name = get_class_name_from_object (object);
  GType type = g_type_from_name (class_name);

  if (webkit_marshall_init ())
    return NULL;

  if (type == G_TYPE_INVALID)
    {
      GParamSpec **properties = clippy_js_proxy_properties_from_js (webview, object);
      type = clippy_js_proxy_class_new (class_name, properties);
    }

  return g_object_new (type, "webview", webview, "object-name", object, NULL);
}

