/* clippify.js
 *
 * Copyright 2018-2019 Endless Mobile, Inc.
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

/*
 * This Immediately Invoked Function Expression add the wrapper nescesary to
 * make Clippy notify system work.
 *
 * This is used as a format string in _js_run_printf()
 */
(function (obj) {

  function setProperty (obj, property, value) {
    /* Set value in proxy object */
    obj._clippy[property] = value;

    /* Notify clippy */
    window.webkit.messageHandlers.clippy_notify.postMessage({
      property: property,
      value: value
    });
  }

  /* Return if this object is clippified */
  if (obj._clippy !== undefined)
    return obj;

  /* Create Clippy proxy object */
  obj._clippy = Object.create(
    Object.getPrototypeOf(obj),
    Object.getOwnPropertyDescriptors(obj)
  );

  for (prop in obj) {
    if (prop[0] === '_')
      continue;

    let property = prop;

    /* Define generic getter */
    let descriptor = {
      get () {
        return this._clippy[property];
      }
    };

    /* Define setter */
    if (typeof obj[property] === 'number') {
      descriptor.set = function (val) {
        /* Compare floating point numbers using EPSILON */
        if (Math.abs (this._clippy[property] - val) < Number.EPSILON)
          return;

        setProperty (this, property, val);
      };
    } else {
      descriptor.set = function (val) {
        if (this._clippy[property] === val)
          return;

        setProperty (this, property, val);
      };
    }

    Object.defineProperty (obj, property, descriptor);
  }

  return obj;
/* printf format string for the object name */
})(%s);

