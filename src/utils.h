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

typedef enum
{
  CLIPPY_OK,
  CLIPPY_UNKNOWN_ERROR,
  CLIPPY_NO_OBJECT,
  CLIPPY_NO_PROPERTY,
  CLIPPY_NO_SIGNAL,
  CLIPPY_NO_DETAIL,
  CLIPPY_NOT_A_WIDGET,
  CLIPPY_WRONG_SIGNAL_TYPE,
  CLIPPY_WRONG_MSG_ID
} ClippyError;

GQuark clippy_quark (void);
#define CLIPPY_ERROR clippy_quark()

#define clippy_return_if_fail(expr,err,code,format,...) \
  if (!(expr)) \
    { \
      g_set_error (err, CLIPPY_ERROR, code, format, __VA_ARGS__); \
      return; \
    }

#define clippy_return_val_if_fail(expr,val,err,code,format,...) \
  if (!(expr)) \
    { \
      g_set_error (err, CLIPPY_ERROR, code, format, __VA_ARGS__); \
      return val; \
    }

const gchar *object_get_name     (GObject      *object);

gboolean     app_get_object_info (GApplication *app,
                                  const gchar  *object,
                                  const gchar  *property,
                                  const gchar  *signal,
                                  GObject     **gobject,
                                  GParamSpec  **pspec,
                                  guint        *signal_id,
                                  GError      **error);

GVariant    *variant_new_value   (const GValue *value);

gboolean     value_set_variant   (GValue       *value,
                                  GVariant     *variant,
                                  GApplication *app);

void         str_replace_char    (gchar        *str,
                                  gchar         a,
                                  gchar         b);

G_END_DECLS
