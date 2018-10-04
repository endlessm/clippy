/* clippy-js-proxy.h
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

#include <glib-object.h>

G_BEGIN_DECLS

#define CLIPPY_TYPE_JS_PROXY (clippy_js_proxy_get_type())

G_DECLARE_DERIVABLE_TYPE (ClippyJsProxy, clippy_js_proxy, CLIPPY, JS_PROXY, GObject)

struct _ClippyJsProxyClass
{
  GObjectClass  parent_class;
};

GParamSpec   **clippy_js_proxy_properties_from_js (GObject     *webview,
                                                   const gchar *object);

GType          clippy_js_proxy_class_new          (const gchar  *name,
                                                   GParamSpec  **properties);

GObject       *clippy_js_proxy_new                (GObject     *webview,
                                                   const gchar *object);

G_END_DECLS

