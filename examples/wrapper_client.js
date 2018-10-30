const {Gio, GLib, GObject} = imports.gi;

var Clippy = Gio.DBusProxy.makeProxyWrapper(`
<node xmlns:doc="http://www.freedesktop.org/dbus/1.0/doc.dtd">
  <interface name='com.endlessm.Clippy'>
    <method name='Export'>
      <arg type='s' name='object' />
      <arg type='s' name='path' direction='out'/>
      <arg type='s' name='info' direction='out'/>
    </method>
  </interface>
</node>
`);
var clippy = new Clippy (Gio.DBus.session,
                         'com.endlessm.clippy.test',
                         '/com/endlessm/clippy');

var [path, info] = clippy.ExportSync ('webview.JSContext.testobject');

print ("Path:", path);
print ("DBus info:\n", info);

var TestObject = Gio.DBusProxy.makeProxyWrapper(info);
var testobject = new TestObject(Gio.DBus.session, 'com.endlessm.clippy.test', path);

print ("Member from proxy object: ");
for (let a in testobject)
  print (a, testobject[a]);

/*
 * Gio.DBusProxy.makeProxyWrapper() does not create an actual GObject class
 */
testobject.connect ('g-properties-changed', (obj, changed_properties, invalidated_properties) => {
  print ("g-properties-changed: ", invalidated_properties);

  for (let i = 0, n = invalidated_properties.length; i < n; i++)
    {
      print (invalidated_properties[i]);
      obj.notify (invalidated_properties[i]);
    }
});

new GLib.MainLoop (null, false).run();

