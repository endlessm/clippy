/* utils.h
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

G_BEGIN_DECLS

const gchar *object_get_name   (GObject      *object);

GObject     *app_get_object    (GApplication *app,
                                const gchar  *name);

GVariant    *variant_new_value (const GValue *value);

gboolean     value_set_variant (GValue       *value,
                                GVariant     *variant);

G_END_DECLS
