## Packer

Plugin to pack and unpack archives on the fly.

Provides `:Pack` command that archives selection or current file.
Name of a single file is used to name the archive, for multiple
files name of current directory is used. `:Pack` optionally takes
an argument.  If the argument is given, it will be used as archive's
name instead of the previous options. e.g. `:Pack foobar.zip`.

Provides `:Unpack` command that unpacks common archives like tarballs,
zip, rar or 7z nicely in a subdirectory.  Supports unpacking multiple files
at once (performed in sequence).

### Features

* Unpack zip, rar, 7z, tar.gz, tar.bz2, etc.
* Unpack multiple archives on the fly
* Archives are unpacked nicely in a subdirectory
* Doesn't overwrite existing files
* Supports unpacking archives with special characters

### Requirements

* `7z`  - p7zip for most of decompression, including rar to
          extract rar files, p7zip must be build with rar support.
          also for packing into zip, 7z, lz4 etc type compressions.
          Why 7z? well it supports wide range of archives, it is
          fast and efficient at decompression.

* `tar` - for (un)packing compressed tarballs, currently supports
          gzip, xz, bzip2 and zstd compressed tarballs.

* `awk` is required as well.

### Commands

#### Unpack

Syntax:
```
:Unpack -b [<dir>]
```

Examples:
```
:Unpack
:Unpack %D
:Unpack /tmp
```

Extracts archive into a subdirectory or `<dir>` if given.  With `-b`, the
command returns after starting to unpack the first archive and schedules the
next one after the first one is done, etc.

#### Pack

Syntax:
```
:Pack [<archive-name>]
```

Examples:
```
:Pack %c.7z
:Pack foobar.tar.gz
```

Archives the selected files.  Optionally takes archive name.

### Status

Operating systems:
 - working under Linux environment (tested on Gentoo, Slackware)
 - working in Windows

Limitations:
 - extracting large tarballs is slow due to `tar tf` being slow

Features under consideration:
 - password support
 - ask to overwrite or merge when (un)packing

TODOs:
 - check presence of the applications
 - displaying progress would be nice
 - a way to specify default path for unpacking
 - test using non-GNU awk
 - replace listing of archive's content with always extracting into subdirectory
   and moving files afterward
 - organize code in object-oriented style around descriptions of archivers with
   means to check for their presence, list of supported formats and command
   patterns for packing/unpacking
