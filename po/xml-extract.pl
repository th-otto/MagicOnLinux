#!/usr/bin/perl -w 

#
#  XML Message Extractor
#
#  Copyright (C) 2025 Thorsten Otto
#
#

## Loaded modules
use strict; 

my $FILE;

my $input;
my %messages = ();
my @messages_sorted = ();
my %loc = ();
my %comments = ();

## Always print first
$| = 1;

my $OUT;
open $OUT, ">>", "/dev/stdout";
# binmode(OUT) if $^O eq 'MSWin32';

while (@ARGV > 0)
{
	$FILE = $ARGV[0];
	&convert;
	shift @ARGV;
}
&msg_write;
# close OUT;


sub convert
{
	## Reading the file
	{
		local(*IN);
		local $/; #slurp mode
		open(IN, "<$FILE") || die "can't open $FILE: $!";
		binmode(IN);
		$input = <IN>;
		close IN;
	}

	### For generic translatable XML files ###
	my $tree = readXml($input);
}

sub escape_char
{
	return '\"' if $_ eq '"';
	return '\n' if $_ eq "\n";
	return '\\\\' if $_ eq '\\';

	return $_;
}

sub escape($)
{
	my ($string) = @_;
	return join "", map &escape_char, split //, $string;
}

sub add_message($$)
{
	my ($string, $lineno) = @_;
	if (!defined($messages{$string}))
	{
		push @messages_sorted, $string;
		$messages{$string} = [];
	}
	push @{$loc{$string}}, $lineno if defined($lineno);
}

sub tree_comment
{
	my $expat = shift;
	my $data = $expat->original_string();
	my $clist = $expat->{Curlist};
	my $pos = $#$clist;

	$data =~ s/^<!--//s;
	$data =~ s/-->$//s;
	# push @$clist, 1 => $data;
}

sub tree_cdatastart
{
	my $expat = shift;
	my $clist = $expat->{Curlist};
	my $pos = $#$clist;

	push @$clist, 0 => $expat->original_string();
}

sub tree_cdataend
{
	my $expat = shift;
	my $clist = $expat->{Curlist};
	my $pos = $#$clist;

	$clist->[$pos] .= $expat->original_string();
}

sub tree_char
{
	my $expat = shift;
	my $text = shift;
	my $clist = $expat->{Curlist};
	my $pos = $#$clist;

	# Use original_string so that we retain escaped entities
	# in CDATA sections.
	#
	if ($pos > 0 and $clist->[$pos - 1] eq '0')
	{
		$clist->[$pos] .= $expat->original_string();
	} else
	{
		push @$clist, 0 => $expat->original_string();
	}
}

sub tree_start
{
	my $expat = shift;
	my $tag = shift;
	my $lineno = $expat->current_line() + 1;
	my $msgctxt = "";
	# print "$lineno: start $tag\n";
	while (my $attr = shift)
	{
		my $value = shift;
		if ($attr eq "msgctxt")
		{
			$msgctxt = "$value\004";
		}
		if ($attr =~ /^_/)
		{
			# print "   $attr: $msgctxt$value\n";
			add_message("$msgctxt$value", $lineno);
		}
	}
}

sub tree_end
{
	my $expat = shift;
	my $tag = shift;
	my $lineno = $expat->current_line() + 1;
	# print "$lineno: end $tag\n";
}

sub readXml
{
	my $xmldoc = shift || return;
	my $ret = eval 'require XML::Parser';
	if (!$ret)
	{
		die "You must have XML::Parser installed to run $0\n\n";
	}
	my $xp = new XML::Parser(Style => 'Tree');
	$xp->setHandlers(Char => \&tree_char);
	$xp->setHandlers(Start => \&tree_start);
	$xp->setHandlers(End => \&tree_end);
	$xp->setHandlers(CdataStart => \&tree_cdatastart);
	$xp->setHandlers(CdataEnd => \&tree_cdataend);

	$xp->setHandlers(Comment => \&tree_comment);

	my $tree = $xp->parse($xmldoc);

	return $tree;
}

sub msg_write
{
	my $filename = $FILE;
	
print << 'EOF';
# SOME DESCRIPTIVE TITLE.
# Copyright (C) YEAR THE PACKAGE'S COPYRIGHT HOLDER
# This file is distributed under the same license as the MagicOnLinux package.
# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.
#
#, fuzzy
msgid  ""
msgstr ""
"Project-Id-Version: PACKAGE VERSION\n"
"Report-Msgid-Bugs-To: \n"
"POT-Creation-Date: YEAR-MO-DA HO:MI+ZONE\n"
"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\n"
"Last-Translator: FULL NAME <EMAIL@ADDRESS>\n"
"Language-Team: LANGUAGE <LL@li.org>\n"
"Language: en_US\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Content-Transfer-Encoding: 8bit\n"

EOF

	for my $message (@messages_sorted)
	{
		my $offsetlines = 1;
		my $context = undef;
		
		$offsetlines++ if $message =~ /%/;
		if (defined($comments{$message}))
		{
			while ($comments{$message} =~ m/\n/g)
			{
				$offsetlines++;
			}
		}
		print $OUT "#. " . $comments{$message} . "\n"
			if (defined($comments{$message}));
		if (defined($loc{$message}))
		{
			for my $lineno (@{$loc{$message}})
			{
				print $OUT "#: $filename:" . ($lineno - $offsetlines) . "\n";
			}
		}
		print $OUT "#, no-c-format\n" if $message =~ /%/;

		if ($message =~ /(.*)\004(.*)/s)
		{
			$context = $1;
			print $OUT "msgctxt \"" . $context . "\"\n"; 
			$message = $2;
		}
		my @lines = split(/\n/, $message, -1);
		print $OUT "msgid  ";
		print $OUT "\"\"\n"
			if (@lines > 1);
		for (my $n = 0; $n < @lines; $n++)
		{
			print $OUT "\""; 
			print $OUT escape($lines[$n]);
			print $OUT "\"\n"; 
		}
		print $OUT "msgstr \"\"\n\n";
	}
}
