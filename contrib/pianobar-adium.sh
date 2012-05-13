#!/bin/bash

cleanup()
{
# unset status on exit
osascript <<EOF
tell application "Adium" to go online with message ""
EOF
reset
}

trap cleanup SIGINT
pianobar
