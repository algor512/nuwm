# NUWM - Nu's Window Manager

## Summary

`nuwm` is a small dynamic window manager for X forked from [catwm](https://github.com/pyknite/catwm).

## Modes

Tiling
```
--------------
|        |___|
|        |___|
| Master |___|
|        |___|
|        |___|
--------------
```
and monocle mode.

## Installation

After making your changes in `config.h`:

```
$ make
# make install
```

By default `nuwm` is installed to `/usr/local/bin`. You can change this in the `Makefile`.

## Todo

- Log desktop info to stdout to be used by bars that work with stdin
- Multiple desktop view
- Window rules
- Increase/decrease number of master windows
