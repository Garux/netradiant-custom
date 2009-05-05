#!/usr/bin/perl

use strict;
use warnings;

sub ParseEntity($)
{
	my ($fh) = @_;

	my %ent = ( );
	my @brushes = ( );

	while(<$fh>)
	{
		chomp; s/\r//g; s/\0//g; s/\/\/.*$//; s/^\s+//; s/\s+$//; next if /^$/;

		if(/^\{$/)
		{
			# entity starts
			while(<$fh>)
			{
				chomp; s/\r//g; s/\0//g; s/\/\/.*$//; s/^\s+//; s/\s+$//; next if /^$/;

				if(/^"(.*?)" "(.*)"$/)
				{
					# key-value pair
					$ent{$1} = $2;
				}
				elsif(/^\{$/)
				{
					my $brush = [];
					push @brushes, $brush;

					while(<$fh>)
					{
						chomp; s/\r//g; s/\0//g; s/\/\/.*$//; s/^\s+//; s/\s+$//; next if /^$/;

						if(/^\{$/)
						{
							# patch?
							push @$brush, $_;

							while(<$fh>)
							{
								chomp; s/\r//g; s/\0//g; s/\/\/.*$//; s/^\s+//; s/\s+$//; next if /^$/;

								if(/^\}$/)
								{
									push @$brush, $_;

									last;
								}
								else
								{
									push @$brush, $_;
								}
							}
						}
						elsif(/^\}$/)
						{
							# end of brush
							last;
						}
						else
						{
							push @$brush, $_;
						}
					}
				}
				elsif(/^\}$/)
				{
					return \%ent, \@brushes;
				}
			}
		}
		else
		{
			die "Unexpected line in top level: >>$_<<";
		}
	}

	return undef;
}

sub UnparseEntity($$)
{
	my ($ent, $brushes) = @_;
	my %ent = %$ent;

	my $s = "{\n";

	for(sort keys %ent)
	{
		$s .= "\"$_\" \"$ent{$_}\"\n";
	}

	if(defined $brushes)
	{
		for(@$brushes)
		{
			$s .= "{\n";
			$s .= "$_\n" for @$_;
			$s .= "}\n";
		}
	}

	$s .= "}\n";
	return $s;
}

my @axialbrushpattern = (
	[ "+++", "+-+", "-++", " - ", "-  " ],
	[ "+++", "-++", "++-", "+  ", "  +" ],
	[ "+++", "++-", "+-+", " - ", "  +" ],
	[ "---", "+--", "-+-", " - ", "+  " ],
	[ "---", "--+", "+--", "-  ", "  +" ],
	[ "---", "-+-", "--+", " + ", "  +" ]
);
sub axialbrushpattern($$$)
{
	my ($plane, $vertex, $coord) = @_;
	my $ch = substr $axialbrushpattern[$plane][$vertex], $coord, 1;
	return $ch eq '+' ? +1 : $ch eq '-' ? -1 : 0;
}
sub frac($)
{
	my ($x) = @_;
	return $x - int $x;
}
sub ConvertOriginBrush($$$$)
{
	my ($brushPrimit, $x, $y, $z) = @_;
	my @data = ();
	if($brushPrimit)
	{
		push @data, "brushDef";
		push @data, "{";
		for(0..5)
		{
			push @data, sprintf
				"( %s %s %s ) ( %s %s %s ) ( %s %s %s ) ( ( %s %s %s ) ( %s %s %s ) ) common/origin 0 0 0",
				$x + 8 * axialbrushpattern($_, 0, 0), $y + 8 * axialbrushpattern($_, 0, 1), $z + 8 * axialbrushpattern($_, 0, 2),
				$x + 8 * axialbrushpattern($_, 1, 0), $y + 8 * axialbrushpattern($_, 1, 1), $z + 8 * axialbrushpattern($_, 1, 2),
				$x + 8 * axialbrushpattern($_, 2, 0), $y + 8 * axialbrushpattern($_, 2, 1), $z + 8 * axialbrushpattern($_, 2, 2),
				1/16.0, 0, frac((axialbrushpattern($_, 3, 0) * $x + axialbrushpattern($_, 3, 1) * $y + axialbrushpattern($_, 3, 2) * $z) / 16.0 + 0.5),
				0, 1/16.0, frac((axialbrushpattern($_, 4, 0) * $x + axialbrushpattern($_, 4, 1) * $y + axialbrushpattern($_, 4, 2) * $z) / 16.0 + 0.5);
		}
		push @data, "}";
	}
	else
	{
		my $data = "// origin brush\n{\n";
		for(0..5)
		{
			push @data, sprintf
				"( %s %s %s ) ( %s %s %s ) ( %s %s %s ) common/origin %s %s 0 %s %s 0 0 0",
				$x + 8 * axialbrushpattern($_, 0, 0), $y + 8 * axialbrushpattern($_, 0, 1), $z + 8 * axialbrushpattern($_, 0, 2),
				$x + 8 * axialbrushpattern($_, 1, 0), $y + 8 * axialbrushpattern($_, 1, 1), $z + 8 * axialbrushpattern($_, 1, 2),
				$x + 8 * axialbrushpattern($_, 2, 0), $y + 8 * axialbrushpattern($_, 2, 1), $z + 8 * axialbrushpattern($_, 2, 2),
				frac((axialbrushpattern($_, 3, 0) * $x + axialbrushpattern($_, 3, 1) * $y + axialbrushpattern($_, 3, 2) * $z) / 16.0 + 0.5) * 64.0,
				frac((axialbrushpattern($_, 4, 0) * $x + axialbrushpattern($_, 4, 1) * $y + axialbrushpattern($_, 4, 2) * $z) / 16.0 + 0.5) * 64.0,
				1/4.0, 1/4.0;
		}
	}
	return \@data;
}

my ($infile, $outfile) = @ARGV;
open my $infh, '<', $infile
	or die "<$infile: $!";
my $brushPrimit = 0;

my $outbuf = "";
for(;;)
{
	my ($ent, $brushes) = ParseEntity $infh;
	defined $ent
		or last;
	if(@$brushes)
	{
		$brushPrimit = 1
			if grep { m!\s+brushDef\s+!; } @$brushes;
		if(grep { m!\s+common/origin\s+!; } @$brushes)
		{
			# we have origin brushes - good
		}
		else
		{
			if(defined $ent->{origin})
			{
				my $origin = [ split /\s+/, ($ent->{origin} || "0 0 0") ];
				delete $ent->{origin};
				push @$brushes, ConvertOriginBrush $brushPrimit, $origin->[0], $origin->[1], $origin->[2];
			}
		}
	}
	$outbuf .= UnparseEntity $ent, $brushes;
}

close $infh;

open my $outfh, '>', $outfile
	or die ">$outfile: $!";
print $outfh $outbuf;
close $outfh;
