#!/usr/bin/perl -w

# proc_map.pl
#
# Parses GCC linker map file into a format that we can load as a kernel module into our kernel.
# Our kernel can then use it when creating a stack dump (to resolve symbols based on virtual
# addresses).

use strict;

my $state = 0;
my $section_name = "";
my $section_addr = "";
my $symbol = "";
my $addr = "";

my %text;

while (<STDIN>) {
	chomp(my $line = $_);

	if ($state == 0) {
		if ($line =~ m/^\s?(\.[\.a-z0-9\-_]*)\s+0x([0-9a-f]{16})/) {
			$section_name = $1;
			$section_addr = $2;

			if ($section_name =~ m/\.text$/) {
#				printf "// $section_name, $section_addr\n";
				$state = 1;
			}
		}
	} elsif ($state == 1) {
		if ($line =~ m/^\s?(\.[\.a-z0-9\-_]*)\s+0x([0-9a-f]{16})/) {
			$section_name = $1;
			$section_addr = $2;

			if ($section_name =~ m/\.text$/) {
#				printf "// $section_name, $section_addr\n";
				$state = 1;
			}
		} elsif	($line =~ m/^\s+0x([0-9a-f]{16})\s+([a-z0-9_]+)/) {
			$addr = $1;
			$symbol = $2;

			$text{$addr} = $symbol;

#			printf "// $section_name, $symbol, $addr\n";
		} else {
			$state = 0;
		}
	}
}


print "/***** Generated Code.  Do not edit. *****/\n\n";
print "static const struct symbol_t symbols[] =\n{\n";

foreach my $key (sort keys %text) {
	my $addr64 = $key;
	my $symbol = $text{$key};

	my $addr32 = substr($addr64, 8, 8);

	print "\t{ 0x$addr32, \"$symbol\" },\n";
}

print "\t{ 0, NULL }\n};\n\n";
