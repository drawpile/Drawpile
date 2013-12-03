Textual command stream format
------------------------------

The textual command stream format is like a session recording,
except in human readable text format. It is mainly intended
for test scripts.

The file consists of commands, one per line, and their arguments. Empty lines
and comments starting with '#' are allowed.


## Supported commands

### ctx

Usage: ctx <ctxId> param=value...

This command generates a tool context change. The loader will remember
generated contexts, so specifying only the parameters that change is allowed.
Supported parameters are:

 * layer = layer ID
 * blend = blending mode (SVG type name)
 * hardedge = <true/false> hard edge mode
 * incremental = <true/false> incremental drawing mode
 * spacing = dab spacing in percents
 * colorh = high pressure color (#rrggbb)
 * colorl = low pressure color
 * hardh = high pressure hardness
 * hardl = low pressure hardness
 * sizeh = high pressure size
 * sizel = low pressure size
 * opacityh = high pressure opacity
 * opacityl = low pressure opacity

### move

Usage: move <ctxId> <x> <y> [p][; <x> <y> [p];...]

This command generates a pen move. The pressure paramter
is optional. If omitted, a pressure of 1.0 will be used.

### penup

Usage: penup <ctxId>

This command generates a penup event for the given drawing context.

### resize <w> <h>

This command generates a canvas resize message. Currently it can only
be used once in the very beginning of the file.

### newlayer

Usage: newlayer <layerId> <fill> [title]

This command creates a new layer. The fill color should be in format #rrggbbaa.

### layerattr

Usage: layerattr <layerId> *parameters

This command changes layer attributes. The loader will remember the layer's old
attributes, so specifying only the parameters that change is allowed.
Supported parameters are:

 * opacity - layer opacity (0.0 - 1.0)
 * blend   - layer blending mode (SVG type name)

### retitlelayer

Usage: retitlelayer <layerId> <title>

This command changes a layer's title

### deletelayer

Usage: deletelayer <id> [merge]

This command deletes a layer. If the parameter "merge" is present, the
layer will be merged with the one below it

### layerorder

Usage: layerorder <id1> <id2> ...

This command reorders the layers. All existing layers should be listed
in bottom to top order.

### addannotation

Usage: addannotation <ctxId> <aId> <x> <y> <w> <h>

Create a blank new annotation by user <ctx>

### reshapeannotation

Usage: reshapeannotation <aId> <x> <y> <w> <h>

Change an existing annotations position and size

### annotate / endannotate

Usage:
	annotate <aId> [bg=<color>]
	...
	endannotate

Change the contents of an annotation. This is a multiline command. Everything
between annotate and endannotate will be used as the annotation content.

### deleteannotation

Usage: deleteannotation <aId>

Delete an annotation

### putimage

Usage: putimage <layerId> <x> <y> [blend] <filename>

Load an image and draw it onto the given canvas at the specified location.
If the "blend" parameter is set, the image is alpha blended. Otherwise the
image will simply replace the existing pixels.
The file name is relative to the path of the command file.

