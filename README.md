# nuwm - Nu's Window Manager

## Summary

`nuwm` is a small dynamic window manager for X.

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

## Bugs

I'm working on fixing the following bugs:

- dunst notifications get placed behind all windows when switching desktops

## Todo

- Multiple desktop view
- Window rules
- Increase/decrease number of master windows function
- Log desktop info to stdout to be used by bars that work with stdin
- Multiple monitors support

# References

- CatWM - [catwm](https://github.com/pyknite/catwm) (start point of this project)
- MonsterWM - [monsterwm](https://github.com/c00kiemon5ter/monsterwm)
- Dynamic Window Manager by Suckless - [dwm](https://git.suckless.org/dwm/)
- Calm Window Manager by OpenBSD - [cwm](https://github.com/openbsd/xenocara/tree/master/app/cwm)
- Berry - [berrywm](https://github.com/JLErvin/berry)
