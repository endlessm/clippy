#!/bin/bash

function run ()
{
  echo $@
  gdbus call --session --dest org.gnome.gedit --object-path /org/gnome/gedit --method "$@"
}

LD_PRELOAD=./libclippy-0.1.so G_MESSAGES_DEBUG=1 gedit&

sleep 1
run com.endlessm.Clippy.Highlight open_button

sleep 1
run com.endlessm.Clippy.Clear

run com.endlessm.Clippy.Set open_button label "<'Hola Mundo'>"

run com.endlessm.Clippy.Get open_button label

run com.endlessm.Clippy.Connect open_button clicked nothing

run com.endlessm.Clippy.Emit open_button activate nothing "<('',)>"

sleep 1
run com.endlessm.Clippy.Set search_entry text "<'xxHola Mundo'>"

sleep 1
run com.endlessm.Clippy.Emit search_entry delete-from-cursor nothing "<(0, 2)>"

sleep 2
run org.gtk.Actions.Activate 'quit' [] {}