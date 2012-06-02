#!/bin/sh

if [ $1 == "songstart" ]; then
    SONG=`awk -F= '/^artist/ {printf $2 " ~ "} /^title/ {printf $2}'`
    purple-remote "setstatus?message=♫ $SONG"
fi
