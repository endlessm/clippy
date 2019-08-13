#!/bin/bash

function run ()
{
  echo $@
  gdbus call --session --dest com.endlessm.clippy.test --object-path /com/endlessm/clippy --method "$@"
}

G_MESSAGES_DEBUG=1 $1&

sleep 2

run com.hack_computer.Clippy.Set webview.JSContext.testobject astring "<'Hola Mundo DBus'>"