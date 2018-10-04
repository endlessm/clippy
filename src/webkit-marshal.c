/* webkit-marshal.c
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

#include "webkit-marshal.h"
#include <gtk/gtk.h>

typedef struct
{
  GType
  (* webkit_web_view_get_type) (void);

  WebKitJavascriptResult *
  (* webkit_web_view_run_javascript_finish) (WebKitWebView *web_view,
                                             GAsyncResult *result,
                                             GError **error);

  WebKitJavascriptResult *
  (* webkit_javascript_result_ref)          (WebKitJavascriptResult *js_result);

  void
  (* webkit_javascript_result_unref)        (WebKitJavascriptResult *js_result);

  void
  (* webkit_web_view_run_javascript)        (WebKitWebView *web_view,
                                             const gchar *script,
                                             GCancellable *cancellable,
                                             GAsyncReadyCallback callback,
                                             gpointer user_data);
  JSCValue *
  (* webkit_javascript_result_get_js_value) (WebKitJavascriptResult *js_result);

  WebKitUserContentManager *
  (* webkit_web_view_get_user_content_manager) (WebKitWebView *web_view);

  gboolean
  (* webkit_user_content_manager_register_script_message_handler) (WebKitUserContentManager *manager,
                                                                   const gchar *name);

  gboolean   (* jsc_value_is_boolean) (JSCValue *value);
  gboolean   (* jsc_value_to_boolean) (JSCValue *value);
  gboolean   (* jsc_value_is_string)  (JSCValue *value);
  char *     (* jsc_value_to_string)  (JSCValue *value);
  gboolean   (* jsc_value_is_number)  (JSCValue *value);
  double     (* jsc_value_to_double)  (JSCValue *value);
  gboolean   (* jsc_value_is_object)  (JSCValue *value);
  JSCValue * (* jsc_value_object_get_property)         (JSCValue   *value,
                                                        const char *name);
  gchar **   (* jsc_value_object_enumerate_properties) (JSCValue   *value);
} WebkitSymbols;

static WebkitSymbols webkit = { NULL, };

#define WEBKIT_CALL(func,...) \
  return webkit.func (__VA_ARGS__)

#define DEFINE_JSC_TYPE(t,t2,rt) \
gboolean \
_clippy_jsc_value_is_##t (JSCValue *value) \
{ \
  return webkit.jsc_value_is_##t(value); \
} \
rt \
_clippy_jsc_value_to_##t2 (JSCValue *value) \
{ \
  return webkit.jsc_value_to_##t2 (value); \
}

#define GET_SYMBOL(s) g_module_symbol (main_module, #s, (gpointer)&webkit.s)

gboolean
webkit_marshall_init ()
{
  static gsize initialization_value = 0;

  if (g_once_init_enter (&initialization_value))
    {
      GModule *main_module = g_module_open (NULL,  G_MODULE_BIND_LOCAL);
      gsize setup_value = TRUE;

      GET_SYMBOL (webkit_web_view_get_type);
      GET_SYMBOL (webkit_web_view_run_javascript_finish);
      GET_SYMBOL (webkit_javascript_result_ref);
      GET_SYMBOL (webkit_javascript_result_unref);
      GET_SYMBOL (webkit_web_view_run_javascript);
      GET_SYMBOL (webkit_javascript_result_get_js_value);
      GET_SYMBOL (webkit_web_view_get_user_content_manager);
      GET_SYMBOL (webkit_user_content_manager_register_script_message_handler);

      GET_SYMBOL (jsc_value_is_boolean);
      GET_SYMBOL (jsc_value_to_boolean);
      GET_SYMBOL (jsc_value_is_string);
      GET_SYMBOL (jsc_value_to_string);
      GET_SYMBOL (jsc_value_is_number);
      GET_SYMBOL (jsc_value_to_double);
      GET_SYMBOL (jsc_value_is_object);
      GET_SYMBOL (jsc_value_object_get_property);
      GET_SYMBOL (jsc_value_object_enumerate_properties);

      g_once_init_leave (&initialization_value, setup_value);
    }

  return webkit.webkit_web_view_get_type == NULL;
}

GType
_clippy_webkit_web_view_get_type (void)
{
  if (webkit_marshall_init ())
    return G_TYPE_INVALID;

  return webkit.webkit_web_view_get_type ();
}

WebKitJavascriptResult *
_clippy_webkit_web_view_run_javascript_finish (WebKitWebView *web_view,
                                               GAsyncResult  *result,
                                               GError       **error)
{
  return webkit.webkit_web_view_run_javascript_finish (web_view, result, error);
}

WebKitJavascriptResult *
_clippy_webkit_javascript_result_ref (WebKitJavascriptResult *js_result)
{
  return webkit.webkit_javascript_result_ref (js_result);
}

void
_clippy_webkit_javascript_result_unref (WebKitJavascriptResult *js_result)
{
  webkit.webkit_javascript_result_unref (js_result);
}

void
_clippy_webkit_web_view_run_javascript (WebKitWebView      *web_view,
                                        const gchar        *script,
                                        GCancellable       *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer            user_data)
{
  webkit.webkit_web_view_run_javascript (web_view, script, cancellable, callback, user_data);
}

JSCValue *
_clippy_webkit_javascript_result_get_js_value (WebKitJavascriptResult *js_result)
{
  return webkit.webkit_javascript_result_get_js_value (js_result);
}

WebKitUserContentManager *
_clippy_webkit_web_view_get_user_content_manager (WebKitWebView *web_view)
{
  return webkit.webkit_web_view_get_user_content_manager (web_view);
}

gboolean
_clippy_webkit_user_content_manager_register_script_message_handler (WebKitUserContentManager *manager,
                                                                     const gchar              *name)
{
  return webkit.webkit_user_content_manager_register_script_message_handler (manager, name);
}

DEFINE_JSC_TYPE(boolean, boolean, gboolean);

DEFINE_JSC_TYPE(string, string, gchar *);

DEFINE_JSC_TYPE(number, double, gdouble);

gboolean
_clippy_jsc_value_is_object (JSCValue *value)
{
  return webkit.jsc_value_is_object (value);
}

JSCValue *
_clippy_jsc_value_object_get_property (JSCValue *value, const char *name)
{
  return webkit.jsc_value_object_get_property (value, name);
}

gchar **
_clippy_jsc_value_object_enumerate_properties (JSCValue *value)
{
  return webkit.jsc_value_object_enumerate_properties (value);
}

