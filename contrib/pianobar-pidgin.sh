#!/bin/bash

cleanup()
{
# unset status on exit
purple-remote "setstatus?message="
stty "$term"
}

term=$(stty -g)
trap cleanup SIGINT
pianobar
cleanup
