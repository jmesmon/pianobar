#!/usr/bin/osascript

on run argv
	if item 1 of argv is "songstart" then
		if application "Adium" is running then
			set song to do shell script "awk -F= '/^artist/ {printf $2 \" ~ \"} /^title/ {printf $2}'"
			tell application "Adium" to go online with message "♫ " & song
		end if
	end if
end run
