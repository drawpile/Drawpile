#!/usr/bin/env perl
# SPDX-License-Identifier: GPL-3.0-or-later
# Creates a preset file for src/desktop/assets/brushes. Will query the given
# brush database with the given SQL where condition and dump the resulting
# presets into the file, using zstd compression prefixed by a little-endian 32
# bit unsigned length for use with DP_decompress_zstd.
use strict;
use warnings;
use autodie;

if (@ARGV != 2) {
    die "Usage: $0 PATH_TO_BRUSHES_DB WHERE_CONDITION\n";
}

if (-t STDOUT) {
    die "Refusing to output to a terminal\n";
}

my ($path_to_brushes_db, $where_condition) = @ARGV;

open my $sqlite3, '-|', 'sqlite3', $path_to_brushes_db, <<END_OF_SQL;
select
    format(
        '{"type":"%s","name":"%s","description":"%s","data":%s,"thumbnail":"%s","tags":["Drawpile"]}',
        p.type,
        p.name,
        p.description,
        p.data,
        replace(replace(base64(p.thumbnail), char(10), ''), char(13), ''))
from preset p
left join preset_tag pt on pt.preset_id = p.id
where $where_condition
order by p.name;
END_OF_SQL

my $in = do { local $/; <$sqlite3> };
close $sqlite3;

print pack 'V', length $in;

open my $zstd, '|-', 'zstd';
print {$zstd} $in;
close $zstd;
