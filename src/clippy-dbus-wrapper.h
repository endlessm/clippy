/* clippy-dbus-wrapper.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define CLIPPY_DBUS_WRAPPER_IFACE "com.endlessm.Clippy.Object"

#define CLIPPY_TYPE_DBUS_WRAPPER (clippy_dbus_wrapper_get_type())

G_DECLARE_FINAL_TYPE (ClippyDbusWrapper, clippy_dbus_wrapper, CLIPPY, DBUS_WRAPPER, GDBusInterfaceSkeleton)

ClippyDbusWrapper *clippy_dbus_wrapper_new (GObject      *object,
                                            const gchar  *object_path);

G_END_DECLS
