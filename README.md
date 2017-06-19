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
contents of the `usr/src/minix` directory in the repo to your own
`/usr/src/minix/` directory. To build everything, `cd` to `/usr/src/releasetools`
and run `make hdboot`. This will build your boot images in `/boot`. Reboot the
system and run `ps ax | grep ls` to ensure `ls` is running (its pid should be
`13`).

## Usage

The C library has been modified to include an API for calls to the server. Inside
`/usr/src/minix/include/minix/ls.h`, all available functions are defined and
clearly documented. You can take a look at `ls-api-test` in the root of the repo
for an example of usage (it doubles as a crude test for the server). You will
also find an example `/etc/logs.conf` file where you need to define the available
loggers.

### High-level overview

Very briefly: you define *loggers*, which are actually log destinations that
clients can write to. Each logger has a default severity level (below which
messages are not logged). The actual severity level for a log can be changed via
the API. Each logger can log to stdout, stderr, or a file. Due to the
requirements of the college project this was written for, stdout and stderr exist
as logging destinations, but both actually point to the kernel log :) Each logger
has a name which uniquely identifies it, and file loggers have an `append` flag
which dictates if the file should be truncated when the logger is open, or if the
log messages are appended to it.

Loggers are first open by processes, and then written to. Only one process can
have a logger open at a time.

### The configuration file

In the config file, you define different loggers like this:

```
logger YourLoggerName {
    option1 = value1
    option2 = value2
    ...
}
```

You can define arbitrarily many loggers. The options that can be set on each one
are as follows:

* `destination`. Can be `stderr`, `stdout`, or `file`. If set to `file`, you must
  provide a value for the `filename` option.
* `filename`. Only valid if `destination = file`. Specifies a filename where the
  logs for this logger should be written.
* `append`. Only valid if `destination = file`. Can be `true` or `false`.
  Specifies whether the logs should be appended to the file when the logger is
  open, or if the file should be truncated every time.
* `severity`. Can be `trace`, `debug`, `info` or `warn`. Messages below this
  level will not be written to the log (unless changed using
  `minix_ls_set_logger_level`).
* `format`. How to format each line in the log. You can set any string, using the
  following escape sequences:
    * `%n`: Name of the process writing to the log.
    * `%t`: Current date and time.
    * `%l`: Severity level of the message (`trace`, `debug`, `info` or `warn`).
    * `%m`: Log message provided by the call to `minix_ls_write_log`.
    * `%%`: Literal `%` sign.

## License

The MINIX code contained in this repo is copyrighted by The MINIX project and
release under the MINIX license. I claim no ownership of it.

My own code on top of the MINIX source code is also released under the same MINIX
license (clone of the BSD license).
