# libmypaint

This is a bundled version of [libmypaint 1.6.1](https://github.com/mypaint/libmypaint/tree/v1.6.1) with very minor patches to [mypaint-brush.c](mypaint-brush.c) to remove a pointless glib include and the dependency on json-c.

The files [mypaint-brush-settings-gen.h](mypaint-brush-settings-gen.h) and [brushsettings-gen.h](brushsettings-gen.h) can be regenerated from [brushsettings.json](brushsettings.json) by running [generate.sh](generate.sh), which in turn runs [generate.py](generate.py).

License: ISC, see [COPYING](COPYING) for details.
