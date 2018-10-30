#include <webkit2/webkit2.h>
#include "clippy-js-proxy.h"

static void
test_proxy ()
{
  GParamSpec *properties[8] = { NULL, };
  g_autoptr(GObject) object = NULL;
  g_autofree gchar *string = NULL;
  gdouble double_val;
  gboolean boolean;
  GType my_class;
  gint integer;

  properties[0] = NULL; /* Prop 0 is reserved for GObject */
  properties[1] = g_param_spec_string  ("string",  "", "", NULL, G_PARAM_READWRITE);
  properties[2] = g_param_spec_boolean ("boolean", "", "", FALSE, G_PARAM_READWRITE);
  properties[3] = g_param_spec_int     ("integer", "", "", -G_MAXINT, G_MAXINT, 0, G_PARAM_READWRITE);
  properties[4] = g_param_spec_double  ("double",  "", "", -G_MAXDOUBLE, G_MAXDOUBLE, 0, G_PARAM_READWRITE);

  my_class = clippy_js_proxy_class_new ("JuanPablo", properties);

  object = g_object_new (my_class,
                         "string", "Hola Mundo",
                         "boolean", TRUE,
                         "integer", 1234567,
                         "double", 1.234567,
                         NULL);

  g_object_get (object,
                "string", &string,
                "boolean", &boolean,
                "integer", &integer,
                "double", &double_val,
                NULL);

  g_assert (g_strcmp0 (string, "Hola Mundo") == 0);
  g_assert (boolean == TRUE);
  g_assert (integer == 1234567);
  g_assert (double_val == 1.234567);
}

static void
on_object_notify (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (gobject, pspec->name, &value);

  if (pspec->value_type == G_TYPE_BOOLEAN)
    g_message ("%s %s %s", __func__, pspec->name, (g_value_get_boolean (&value)) ? "True" : "False");
  else if (pspec->value_type == G_TYPE_STRING)
    g_message ("%s %s %s", __func__, pspec->name, g_value_get_string (&value));
  else if (pspec->value_type == G_TYPE_DOUBLE)
    g_message ("%s %s %lf", __func__, pspec->name, g_value_get_double (&value));
  else
    g_message ("%s %s", __func__, pspec->name);
}

static void
on_web_view_load_changed (WebKitWebView  *webview,
                          WebKitLoadEvent load_event,
                          gpointer        user_data)
{
  g_autoptr(GObject) object = NULL;
  g_autofree gchar *string = NULL;
  gboolean boolean;
  gdouble double_val;

  if (load_event != WEBKIT_LOAD_FINISHED)
    return;

  object = clippy_js_proxy_new (G_OBJECT (webview), "testobject");

  g_object_unref (object);

  object = clippy_js_proxy_new (G_OBJECT (webview), "testobject");

  g_object_get (object,
                "a-string", &string,
                "a-boolean", &boolean,
                "a-double", &double_val,
                NULL);

  g_assert (g_strcmp0 (string, "A string") == 0);
  g_assert (boolean == TRUE);
  g_assert (double_val == 1.2345);

  g_object_set (object,
                "a-string", "Juan Pablo",
                "a-boolean", FALSE,
                "a-double", 3.141516,
                NULL);

  g_object_get (object,
                "a-string", &string,
                "a-boolean", &boolean,
                "a-double", &double_val,
                NULL);

  g_assert (g_strcmp0 (string, "Juan Pablo") == 0);
  g_assert (boolean == FALSE);
  g_assert (double_val == 3.141516);

  g_object_set_data_full (G_OBJECT (webview),
                          "ClippyJsProxy",
                          g_object_ref (object),
                          g_object_unref);

  g_signal_connect (object, "notify", G_CALLBACK (on_object_notify), NULL);
}

static void
activate (GApplication *application)
{
  GtkWidget *window, *webview;
  WebKitSettings *settings;
  WebKitWebInspector *inspector;

  window = gtk_application_window_new (GTK_APPLICATION (application));

  webview = webkit_web_view_new ();
  gtk_widget_set_name (webview, "webview");
  gtk_container_add (GTK_CONTAINER (window), webview);

  /* Enable the developer extras */
  settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview));
  g_object_set (G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);

  inspector = webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (webview));
  webkit_web_inspector_show (WEBKIT_WEB_INSPECTOR (inspector));

  test_proxy ();

  /* Wait until JS runs before testing clippy_js_proxy_properties_from_js() */
  g_signal_connect (webview, "load-changed", G_CALLBACK (on_web_view_load_changed), NULL);

  webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview),
                             "<html>"
                             " <script language=\"JavaScript\">"
                             "  var testobject = { a_boolean: true, a_string: 'A string', a_number: 1234, a_double: 1.2345, astring: 'A string' };"
                             " </script>"
                             " <h1>ClippyJSProxy Test</h1>"
                             "</html>",
                             "file://");

  gtk_application_add_window (GTK_APPLICATION (application),
                              GTK_WINDOW (window));
  gtk_widget_show_all (GTK_WIDGET (window));
}

extern void
gtk_module_init(int *argc, char ***argv);

int
main (int argc, char *argv[])
{
  g_autoptr(GtkApplication) app;

  gtk_module_init (&argc, &argv);

  app = gtk_application_new ("com.endlessm.clippy.test",
                             G_APPLICATION_FLAGS_NONE);

  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}

