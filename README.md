# Clippy - it clips things together!

This library can be used to expose Gtk applications internals for scripting
engines to implement interactive lessons in real time in the apps themself.

### How does it work?

This is archived by exposing a generic D-Bus interface that allows you to
highlight a widget to call for user attention, set any object property, emit
action signals or listen for events.

In order to use it all you have to do is preload the library before you run any
Gtk application.

`LD_PRELOAD=./libclippy-0.1.so gedit`

The only requirement is for the application to have a default GApplication which
will be used to know in which D-Bus connection and object path Clippy should 
expose com.hack_computer.Clippy interface.

```shell
gdbus call --session --dest org.gnome.gedit --object-path /org/gnome/gedit \
      --method com.hack_computer.Clippy.Highlight open_button
```

### API

[D-Bus interface definition](https://github.com/endlessm/clippy/blob/master/src/dbus.xml)

### Source repository

[https://github.com/endlessm/clippy](https://github.com/endlessm/clippy)

### Building
 * Install [meson] and [ninja]
 * `$ sudo apt-get install meson`
 * Create a build directory:
 * `$ mkdir _build && cd _build`
 * Run meson:
 * `$ meson`
 * Run ninja:
 * `$ ninja`
 * `$ sudo ninja install`

### Licensing
Clippy is released under the terms of the GNU Lesser General Public License,
either version 2.1 or, at your option, any later version.

[meson]: http://mesonbuild.com/
[ninja]: https://ninja-build.org/