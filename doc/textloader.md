Textual command stream format
------------------------------

The textual command stream format is like a session recording,
except in human readable text format. It is mainly intended
for test scripts.

The file consists of commands, one per line, and their arguments. Empty lines
and comments starting with '#' are allowed.


## Supported commands

### ctx

Usage: `ctx ctxId param=value...`

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
 * color = set both color values
 * hardh = high pressure hardness
 * hardl = low pressure hardness
 * hard = set both hardness values
 * sizeh = high pressure size
 * sizel = low pressure size
 * size = set both size values
 * opacityh = high pressure opacity
 * opacityl = low pressure opacity
 * opacity = set both opacity values

### move

Usage: `move ctxId x y [p][; x y [p];...]`

This command generates a pen move. The pressure paramter
is optional. If omitted, a pressure of 1.0 will be used.

### penup

Usage: `penup ctxId`

This command generates a penup event for the given drawing context.

### resize

Usage: `resize ctxId top right bottom left`

This command generates a canvas resize message. The top, right, bottom and left
parameters expand the canvas in their respective directions. Typically, a session
will start with a `resize 1 0 w h 0` command, where *w* and *h* are the initial
width and height of the canvas.

### newlayer

Usage: `newlayer ctxId layerId fill [title]`

This command creates a new layer. The fill color should be in format #rrggbbaa.

### layerattr

Usage: `layerattr ctxId layerId *parameters`

This command changes layer attributes. The loader will remember the layer's old
attributes, so specifying only the parameters that change is allowed.
Supported parameters are:

 * opacity - layer opacity (0.0 - 1.0)
 * blend   - layer blending mode (SVG type name)

### retitlelayer

Usage: `retitlelayer ctxId layerId title`

This command changes a layer's title

### deletelayer

Usage: `deletelayer ctxId id [merge]`

This command deletes a layer. If the parameter "merge" is present, the
layer will be merged with the one below it

### reorderlayers

Usage: `reorderlayers ctxId id1 id2 ...`

This command reorders the layers. All existing layers should be listed
in bottom to top order.

### addannotation

Usage: `addannotation ctxId aId x y w h`

Create a blank new annotation

### reshapeannotation

Usage: `reshapeannotation ctxId aId x y w h`

Change an existing annotations position and size

### annotate / endannotate

Usage:
	annotate ctxId aId bg-color
	...
	endannotate

Change the contents of an annotation. This is a multiline command. Everything
between annotate and endannotate will be used as the annotation content.

### deleteannotation

Usage: `deleteannotation ctxId aId`

Delete an annotation

### inlineimage

Usage:

    inlineimage width height
    ... base64 encoded image data
    ==end==

Define a image content to use with `putimage`. The image data should
be in the same format as used in the protocol.

### putimage

Usage: `putimage ctxId layerId x y replace|blend|under|erase filename`

Load an image and draw it onto the given canvas at the specified location.
The file name is relative to the path of the command file.
The special filename "-" means the latest inline image should be used.

### fillrect

Usage: `fillrect ctxId layerId x y w h color [blendmode]`

Fill a rectangle with solid color, using the given blending mode.

### undopoint

Usage: `undopoint ctxId`

Create an undo point

### undo

Usage: `undo ctxId actions`

Undo or redo the given number of actions. If the number is negative,
the actions are redone.
