# MINIX logger service

This is a fork of the MINIX 3.3.0 source with an implementation for `ls`, a
service that allows other processes to log diagnostic information by simple IPC
calls. `ls` can then redirect this to different files and filter messages that
are below a configured severity level.

## Repo contents

Inside this repo is a snapshot of the `/usr/src/minix` tree with the logger added
inside `/usr/src/minix/servers/ls` and added to all the necessary other files.

## Compiling

Ensure that the source for your Minix system is the same as the one this fork is
based on. You can check the initial commit for differences. Then, copy the
contents of the usr/src/minix directory to your own `/usr/src/minix/directory`. To
build everything, `cd` to `/usr/src/releasetools` and run `make hdboot`. This will
build your boot images in `/boot`. Reboot the system and run `ps ax | grep ls` to
ensure `ls` is running (its pid should be `13`).

## Usage

TODO.

## License

The MINIX code contained in this repo is copyrighted by The MINIX project and
release under the MINIX license. I claim no ownership of it.

My own code on top of the MINIX source code is also released under the same MINIX
license (clone of the BSD license).
