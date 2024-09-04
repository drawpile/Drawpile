#!/usr/bin/env perl
# SPDX-License-Identifier: MIT
use strict;
use warnings;
use File::Find;
use Encode qw(encode decode);

sub slurp {
    my ($path) = @_;
    open my $fh, '<', $path or die "Can't open '$path': $!\n";
    my $content = do { local $/; <$fh> };
    close $fh or die "Can't close '$path': $!\n";
    return decode('UTF-8', $content);
}

sub spew {
    my ($path, $content) = @_;
    open my $fh, '>', $path or die "Can't open '$path': $!\n";
    print {$fh} encode('UTF-8', $content);
    close $fh or die "Can't close '$path': $!\n";
}

sub fix_libs{
    my ($fn, $libs) = @_;
    print "Got libs: '$libs'\n";
    my $result = join ' ', $fn->(split ' ', $libs);
    print "Fixed libs: '$result'\n";
    return "Libs: $result";
}

sub fix_libs_android {
    return grep { $_ ne '-latomic' } @_;
}

sub fix_content_android {
    my ($content) = @_;
    # Fix bogus -latomic that ffmpeg puts into the pc file.
    $content =~ s/^Libs:\s*(.+?)\s*$/fix_libs(\&fix_libs_android, $1)/gme;
    return $content;
}

sub fix_libs_win32 {
    return map { s/\A([^\-].*)\.lib\z/-l$1/r } grep { !/\A-libpath:/ } @_;
}

sub fix_content_win32 {
    my ($content) = @_;
    # Fix broken stuff that ffmpeg puts into the pc file.
    $content =~ s/^Libs:\s*(.+?)\s*$/fix_libs(\&fix_libs_win32, $1)/gme;
    # Fix broken stuff *we* put into the pc file because ffmpeg is broken.
    $content =~ s/^Cflags:\s*-MD\b\s*/CFlags: /g;
    return $content;
}

my %fix_handlers = (
    android => \&fix_content_android,
    win32 => \&fix_content_win32,
);

my $system = shift;
my $fix = $fix_handlers{$system};
if ($fix) {
    print "Fixing pkg-config files for $system\n";
} else {
    die "Don't know how to fix pkg-config files for '$system'";
}

print 'Collecting .pc files in ', join(', ', @ARGV), "\n";
my @paths;
find({wanted => sub { push @paths, $_ if -f && /\.pc\z/ }, no_chdir => 1}, @ARGV);

for my $path (@paths) {
    print "Fixing $path\n";
    my $content = slurp($path);
    spew($path, $fix->($content));
}
