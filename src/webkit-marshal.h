/* webkit-marshal.h
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
#pragma once

#include <gtk/gtk.h>

#define WEBKIT_TYPE_WEB_VIEW (_clippy_webkit_web_view_get_type())
#define WEBKIT_WEB_VIEW(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), WEBKIT_TYPE_WEB_VIEW, WebKitWebView))
#define WEBKIT_IS_WEB_VIEW(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), WEBKIT_TYPE_WEB_VIEW))

typedef GObject WebKitWebView;
typedef GObject WebKitWebInspector;
typedef GObject WebKitJavascriptResult;
typedef GObject WebKitUserContentManager;
typedef GObject WebKitSettings;
typedef GObject JSCValue;

typedef enum {
    WEBKIT_LOAD_STARTED,
    WEBKIT_LOAD_REDIRECTED,
    WEBKIT_LOAD_COMMITTED,
    WEBKIT_LOAD_FINISHED
} WebKitLoadEvent;

#define webkit_web_view_get_type _clippy_webkit_web_view_get_type
#define webkit_web_view_run_javascript_finish _clippy_webkit_web_view_run_javascript_finish
#define webkit_javascript_result_ref _clippy_webkit_javascript_result_ref
#define webkit_javascript_result_unref _clippy_webkit_javascript_result_unref
#define webkit_web_view_run_javascript _clippy_webkit_web_view_run_javascript
#define webkit_javascript_result_get_js_value _clippy_webkit_javascript_result_get_js_value
#define webkit_web_view_get_user_content_manager _clippy_webkit_web_view_get_user_content_manager
#define webkit_user_content_manager_register_script_message_handler _clippy_webkit_user_content_manager_register_script_message_handler

#define DECLARE_JSC_TYPE(t,t2,rt) \
gboolean \
_clippy_jsc_value_is_##t (JSCValue *value); \
rt \
_clippy_jsc_value_to_##t2 (JSCValue *value);

#define jsc_value_is_boolean _clippy_jsc_value_is_boolean
#define jsc_value_to_boolean _clippy_jsc_value_to_boolean
#define jsc_value_is_string  _clippy_jsc_value_is_string
#define jsc_value_to_string  _clippy_jsc_value_to_string
#define jsc_value_is_number  _clippy_jsc_value_is_number
#define jsc_value_to_double  _clippy_jsc_value_to_double
#define jsc_value_is_object  _clippy_jsc_value_is_object
#define jsc_value_object_get_property         _clippy_jsc_value_object_get_property
#define jsc_value_object_enumerate_properties _clippy_jsc_value_object_enumerate_properties


gboolean
webkit_marshall_init (void);

GType
_clippy_webkit_web_view_get_type (void);

WebKitJavascriptResult *
_clippy_webkit_web_view_run_javascript_finish (WebKitWebView *web_view,
                                               GAsyncResult  *result,
                                               GError       **error);
WebKitJavascriptResult *
_clippy_webkit_javascript_result_ref (WebKitJavascriptResult *js_result);

void
_clippy_webkit_javascript_result_unref (WebKitJavascriptResult *js_result);

void
_clippy_webkit_web_view_run_javascript (WebKitWebView *web_view,
                                const gchar *script,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data);

JSCValue *
_clippy_webkit_javascript_result_get_js_value (WebKitJavascriptResult *js_result);

WebKitUserContentManager *
_clippy_webkit_web_view_get_user_content_manager (WebKitWebView *web_view);

gboolean
_clippy_webkit_user_content_manager_register_script_message_handler (WebKitUserContentManager *manager,
                                                                     const gchar              *name);

DECLARE_JSC_TYPE (boolean, boolean, gboolean);

DECLARE_JSC_TYPE (string, string, gchar *);

DECLARE_JSC_TYPE (number, double, gdouble);

gboolean
_clippy_jsc_value_is_object (JSCValue *value);

JSCValue *
_clippy_jsc_value_object_get_property (JSCValue *value,
                                       const char *name);
gchar **
_clippy_jsc_value_object_enumerate_properties (JSCValue *value);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(WebKitWebView, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(WebKitWebInspector, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(WebKitJavascriptResult, webkit_javascript_result_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(WebKitUserContentManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(WebKitSettings, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(JSCValue, g_object_unref)

