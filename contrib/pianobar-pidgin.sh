#!/bin/bash

cleanup()
{
# unset status on exit
purple-remote "setstatus?message="
reset
}

trap cleanup SIGINT
pianobar
