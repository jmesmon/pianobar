#!/bin/bash

cleanup()
{
# unset status on exit
osascript <<EOF
if application "Adium" is running then
	tell application "Adium" to go online with message ""
end if
EOF
reset
}

trap cleanup SIGINT
pianobar
