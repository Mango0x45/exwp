# exwp — Extensible Wallpaper

Exwp is a set of utilities to facilitate the setting of wallpapers on
Wayland.  The end goal of the project is to offer a simple- and minimal
interface for setting global- or per-display wallpapers as efficiently as
possible, while supporting animated transitions between wallpapers.
Animation support is not yet included, but the goal is to support a
plugin system for animations, allowing users to programatically create
their own animations.

This project is composed of the Ewd dæmon, and the Ewctl client
utility.  These utilities are documented in the ewd(1) and ewctl(1)
manuals respectively.  You can also very easily write your own client
programs to communicate with the Ewd dæmon; in fact it is encouraged that
you do so.  Documentation on the protocol used to communicate with the
dæmon can be found in the ewd(7) manual.

## Compilation and Installation

Compiling the project is very simple:

```sh
make
```

Installing it is *also* very simple:

```sh
sudo make install
```

Building will require that you have a compiler that supports the most
basic of C23 features.  C23 is still very young and not well supported,
but GCC is capable of building everything.  The Ewctl utility also has a
dependency on libjxl for handling JPEG XL files.
