Text format recording
---------------------

The recording text format (`.dptxt` file extension) is a human readable(ish)
encoding of a recording. It is useful for testing, debugging, protocol version upgrades,
creating simple session templates, programmatically generating or manipulating recordings
and other things.

File format in a nutshell:

* Blank lines and comments (lines starting with `#` are comments) are ignored.
* `!key=value` lines are metadata. Metadata appearing before the first message are considered
  header metadata.
* The rest of the content are messages, that take the general form of `ctxId message key=value`
* Keys and values may not contain any whitespace. If a value does have whitespace (incl. newlines),
  the multiline format must be used:
      ctxId message key=value {
          key2=line1
          key2=second line
      }
* The penmove command has special syntax: `ctxId penmove x y [p]` (pressure is assumed to be 100% if omitted)
* Values such as opacity and pressure are expressed as percentages.

Examples:

    1 brush color=#ff0000 size=5 opacity=50.0
    1 penmove 0 0 20.0
    1 penmove {
        100 0
        100 100
        0 100
        0 0
    }
    1 penup

For a full list of commands, see `src/shared/net/textmode.cpp` and the message class definitions.

