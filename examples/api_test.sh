#!/bin/bash

function run ()
{
  echo $@
  gdbus call --session --dest org.gnome.gedit --object-path /com/endlessm/clippy --method "$@"
}

sleep 1
run com.hack_computer.Clippy.Highlight open_button 500

sleep 1
run com.hack_computer.Clippy.Highlight open_button 0

sleep 2
run com.hack_computer.Clippy.Unhighlight open_button

run com.hack_computer.Clippy.Message testmsg "You can show messages that disapear automatically" "dialog-information" open_button 1500

sleep 2
run com.hack_computer.Clippy.Message testmsg "Or do not disapear until the user dissmis them" "dialog-information" view 0

sleep 2
run com.hack_computer.Clippy.MessageClear testmsg

run com.hack_computer.Clippy.Connect open_button notify label

run com.hack_computer.Clippy.Connect open_button clicked nothing

run com.hack_computer.Clippy.Set open_button label "<'Hola Mundo'>"

run com.hack_computer.Clippy.Get open_button label

run com.hack_computer.Clippy.Emit activate nothing "<('open_button',)>"

sleep 1
run com.hack_computer.Clippy.Set search_entry text "<'xxHola Mundo'>"

sleep 1
run com.hack_computer.Clippy.Emit delete-from-cursor nothing "<('search_entry', 0, 2)>"

sleep 2
run org.gtk.Actions.Activate 'quit' [] {}
