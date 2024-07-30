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

sub fix_libs {
    my ($libs) = @_;
    print "Got libs: '$libs'\n";
    my $result = join ' ', map { s/\A([^\-].*)\.lib\z/-l$1/r } grep { !/\A-libpath:/ } split ' ', $libs;
    print "Fixed libs: '$result'\n";
    return "Libs: $result";
}

print 'Collecting .pc files in ', join(', ', @ARGV), "\n";
my @paths;
find({wanted => sub { push @paths, $_ if -f && /\.pc\z/ }, no_chdir => 1}, @ARGV);

for my $path (@paths) {
    print "Fixing $path\n";
    my $content = slurp($path);
    $content =~ s/^Libs:\s*(.+?)\s*$/fix_libs($1)/gme;
    spew($path, $content);
}
