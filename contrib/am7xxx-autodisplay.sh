#!/bin/sh
#
# am7xxx-autodisplay - resize the screen and run am7xxx-play
#
# Copyright (C) 2012  Antonio Ospite <ospite@studenti.unina.it>
#
# This program is free software. It comes without any warranty, to
# the extent permitted by applicable law. You can redistribute it
# and/or modify it under the terms of the Do What The Fuck You Want
# To Public License, Version 2, as published by Sam Hocevar. See
# http://sam.zoy.org/wtfpl/COPYING for more details.

# This is just an example script to show how to resize the screen before
# running am7xxx-play, this can be called also from a udev script.
#
# Resizing the screen may be needed if the am7xxx device has problems
# displaying a certain resolution.
#
# For example on some devices the firmware fails to display images of
# resolution 800x469 resulting from scaled down 1024x600 screens, in
# cases like this, resizing the screen can be a viable workaround.

USER=ao2

RESOLUTION_PROJECTOR=800x600
RESOLUTION_ORIGINAL=1024x600

AM7XXX_PLAY=am7xxx-play

# needed when running xrandr as root from udev rules,
# see https://bugs.launchpad.net/ubuntu/+source/xserver-xorg-video-intel/+bug/660901
export XAUTHORITY=$(find /var/run/gdm3/ -type f -path "*${USER}*" 2> /dev/null)

export DISPLAY=:0.0

case $1 in
  start)
    xrandr --size $RESOLUTION_PROJECTOR && \
      $AM7XXX_PLAY -f x11grab -i $DISPLAY
    ;;

  stop)
    xrandr --size $RESOLUTION_ORIGINAL
    ;;

  *)
    { echo "usage: $(basename $0) <start|stop>" 1>&2; exit 1; }
    ;;
esac
