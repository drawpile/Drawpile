#!/usr/bin/python3
"""Drawpile text mode recording reader/writer.
"""

import string
import textwrap

__all__ = ['DpTxtError', 'DpMessage', 'DpTxtReader', 'parse_dptxt_file']

class DpTxtError(Exception):
    pass

class DpMessage:
    """A single Drawpile protocol message.

    Fields:
    ctx:   context (user) ID of the message.
    name:  name of the message
    attrs: dictionary containing the message's attributes,
           except for 'penmove' messages, in which case it's a
           list of (x,y,p) float tuples containing pen coordinates.
           (p is normalized to range [0..1])
    """

    def __init__(self, ctx, name, attrs):
        self.ctx = ctx
        self.name = name
        self.attrs = attrs

    def __str__(self):
        if self.name == '!':
            item = tuple(self.attrs.items())[0]
            out = ('!', item[0], '=', item[1])

        elif self.name == 'penmove':
            out = [str(self.ctx) + ' penmove ']
            if len(self.attrs)>1:
                out.append('{\n')
                for point in self.attrs:
                    out += ('\t', self.__coordinates(point), '\n')
                out += '\t}'

            else:
                out.append(self.__coordinates(self.attrs[0]))

        else:
            short_attrs = {}
            long_attrs = {}
            if self.attrs:
                for k,v in self.attrs.items():
                    val = str(v)
                    if len(val) > 32 or any(ws in val for ws in string.whitespace):
                        long_attrs[k] = val
                    else:
                        short_attrs[k] = val

            out = [str(self.ctx), ' ', self.name]
            for k,v in sorted(short_attrs.items()):
                out += (' ', k, '=', v)

            if long_attrs:
                out.append(' {\n')
                for k,v in sorted(long_attrs.items()):
                    for row in textwrap.wrap(v, 60):
                        out += ('\t', k, '=', row, '\n')
                out += '}'

        return ''.join(out)
    
    def __coordinates(self, point):
        out = '{:.1f} {:.1f}'.format(point[0], point[1])
        if point[2] < 1:
            out += ' {:.3f}'.format(point[2] * 100)
        return out


class DpTxtReader:
    """DpTxt parser functor.

    Usage:
        parse = DpTxtReader()
        for line in file:
            msg = parse(line)
            if msg:
                # do something

    Raises DpTxtError when invalid data is encountered.

    Metadata lines are returned as messages with the name '!'
    and a single key/value pair in the attribs dictionary.
    For convenience, header metadata (metadata that appears
    before any other message) is made available at the
    "header" attribute.

    The attribute "is_header" will be set to False once
    a non-header message is read.
    """

    STATE_EXPECT_CMD = 0
    STATE_EXPECT_KWARGLINE = 1
    STATE_EXPECT_PENMOVE = 2

    def __init__(self):
        self.header = {}
        self.is_header = True

        # Internal parser state:
        self._state = self.STATE_EXPECT_CMD
        self._ctx = None
        self._name = None
        self._kwargs = None
        self._points = None

    def __call__(self, line):
        """Parse a single dptxt line.

        Returns either a DpMessage or None, if more lines are needed to parse
        the entire message.

        For metadata lines, this returns a special message with the
        name "!" and a single key-value pair in its attrs.

        Raises DpTxtError on parse error.
        """
        line = line.strip()
        if not line or line[0] == '#':
            return None

        # Expecting a normal command (or metadata) line: # cmd k=v...
        if self._state == self.STATE_EXPECT_CMD:
            if line[0] == '!':
                # Metadata
                try:
                    k, v = line[1:].split('=', 1)
                except ValueError:
                    raise DpTxtError(line + ": No '=' in metadata line")

                if self.is_header:
                    self.header[k] = v

                return DpMessage(0, '!', {k:v})

            else:
                self.is_header = False

            tokens = line.split()
            if len(tokens) < 2:
                raise DpTxtError(line + ": Expected at least context ID and message name")

            # Extract context ID
            try:
                self._ctx = int(tokens[0])
            except ValueError:
                raise DpTxtError(token + ": Not an integer!")

            if self._ctx < 0 or self._ctx > 255:
                raise DpTxtError(token + ": Context ID out of range! (0-255)")

            # Check if this is a multiline message
            if len(tokens) > 2 and tokens[-1] == '{':
                multiline = True
                tokens = tokens[:-1]
            else:
                multiline = False

            # Get message name
            self.name = tokens[1]

            if self.name == 'penmove':
                # Custom syntax for penmove
                self._kwargs = None
                self._points = []
                if not multiline or len(tokens) > 2:
                    self._points.append(self.parse_penpoint(tokens[2:]))

            else:
                # Other messages have uniform syntax
                self._kwargs = {}
                self._points = None
                for token in tokens[2:]:
                    try:
                        k,v = token.split('=', 1)
                    except ValueError:
                        raise DpTxtError(token + ": Not a key=value pair")
                    self._kwargs[k] = v

            # Expect more?
            if multiline:
                self._state = self.STATE_EXPECT_PENMOVE if self.name == 'penmove' else self.STATE_EXPECT_KWARGLINE
                return None

        elif self._state == self.STATE_EXPECT_PENMOVE:
            # Expect pen point line
            if line != '}':
                self._points.append(self.parse_penpoint(line.split()))
                return None

        elif self._state == self.STATE_EXPECT_KWARGLINE:
            # Expect a key=value line
            if line != '}':
                try:
                    k,v = line.split('=', 1)
                except ValueError:
                    raise DpTxtError(line + ": Not a key=value pair")
                if k in self._kwargs:
                    self._kwargs[k] += v
                else:
                    self._kwargs[k] = v

                return None

        else:
            raise Exception("Invalid state: " + self._state)

        self._state = self.STATE_EXPECT_CMD
        return DpMessage(self._ctx, self.name, self._kwargs or self._points)

    def parse_penpoint(self, tokens):
        if len(tokens) < 2 or len(tokens) > 3:
            raise DpTxtError(' '.join(tokens) + ': expected 2 coordinates and an optional pressure value!')

        x = self.parse_coordinate(tokens[0])
        y = self.parse_coordinate(tokens[1])
        if len(tokens) == 3:
            p = self.parse_coordinate(tokens[2])
            if p < 0 or p > 100:
                raise DpTxtError(tokens[2] + ": pressure must be in range 0-100")
            p = p / 100.0
        else:
            p = 1.0

        return (x,y,p)

    def parse_coordinate(self, token):
        try:
            return float(token)
        except ValueError:
            raise DpTxtError(token + ": Invalid coordinate")


def parse_dptxt_file(infile):
    """A convenience function that parses a dptxt file and
    returns the header metadata (all metadata before the first message)
    and the message list. All post-header metadata is ignored.

    Returns:
    (header, messages)
    """
    messages = []
    linenum = 0
    parse = DpTxtReader()
    for line in infile:
        linenum += 1
        try:
            msg = parse(line)
        except DpTxtError as e:
            raise DpTxtError(str(linenum) + ": " + str(e))

        if msg is not None and msg.name != '!':
            messages.append(msg)

    return parse.header, messages


if __name__ == '__main__':
    import sys
    if len(sys.argv) != 2:
        print(
            "Drawpile text mode recording reader/writer demo.\nUsage: dptxt.py <input.dptxt>",
            file=sys.stderr
        )
        sys.exit(1)

    # Read the dptxt file
    with open(sys.argv[1]) as f:
        try:
            metadata, messages = parse_dptxt_file(f)
        except DpTxtError as e:
            print(sys.argv[1] + ":" + str(e), file=sys.stderr)
            sys.exit(1)

    # Print the same file back out
    for k,v in metadata.items():
        print('!{}={}'.format(k,v))

    print("")
    for msg in messages:
        print(msg)
        if msg.name == 'undopoint':
            print("")

