#!/usr/bin/perl
use strict;
use warnings;
use POSIX qw(strftime);
use IO::Compress::Zip qw($ZipError :constants);

# GLOBALS

# uuid generated with uuidgen on 2023-09-18
my $UUID = 'C06B058C-7B81-41C8-9CAD-2487C884B7BA';
# New creation date each time the script is run
my $DATE = strftime('%Y-%m-%dT%H:%M:%SZ', gmtime time);
# How to name the eBook
my $TITLE = 'Pocket Puzzles';
# Who is the (main) author
my $AUTHOR = 'Steffen Bauer';
my $AUTHOR_SORT = 'Bauer, Steffen';
# Short description
my $DESCRIPTION = 'Rules for the Pocket Puzzles.';

# %games will be a map of game title to game rules
# Additionally the introduction will be in the map
my %games;

# Parse all the games
opendir(my $gamesdir, 'games');
while (my $filename = readdir($gamesdir)) {
    # Ignore all non .c files
    next unless $filename =~ /\.c$/;
    open my $game, '<', "games/$filename";
    my $raw_rules;
    my $name;
    # Get the name and the rules out of the source code
    while (<$game>) {
        # Found the raw rules
        if ( s/^static const char rules\[\]\s*=\s*// .. /\x22;\s*$/) {
            $raw_rules .= eval sprintf($_);
            next;
        }
        # Found the name
        if (/^(const |struct game )+thegame/) {
            $_ = <$game>;
            /^\s*"(.*?)"/;
            $name = $1;
            next;
        }
    }
    # Just in case the name wasn't found…
    if ($name eq "") {
        print $filename,$/;
        die 1;
    }
    # Convert the rules to html
    my $paragraph;
    my $rules;
    foreach (split /(?<=\n)/, $raw_rules) {
        if (/^$/) {
            $rules .= para($paragraph);
            $paragraph = '';
            next;
        }
        $paragraph .= $_;
    }
    $rules .= para($paragraph);
    $games{$name} = $rules;
    close $game;
}
closedir($gamesdir);

# Convert the README to an introductory text in HTML
# NOTE: This is just a cheap parser not capable of all
# markdown possibilities.
open my $readme, '<', 'README.md';
my $intro;
my $paragraph;
my @images;
while (<$readme>) {
    # Handle the code blocks
    if (my $hit = /^```/ .. /^```\s*$/) {
        if ($hit == 1) {
            $intro .= para($paragraph);
            $paragraph = '';
            next;
        }
        if ($hit ne 0+$hit) {
            $intro .= "<pre><code>\n$paragraph</code></pre>";
            $paragraph = '';
            next;
        }
        $paragraph .= $_;
        next;
    }
    # `inline code`
    s:`(.*?)`:<code>$1</code>:g; # NOTE! Cheaply done! Will interpret MD inside code!
    # ''bold**
    s:\*\*(.*?)\*\*:<b>$1</b>:g;
    # *italics*
    s:\*(.*?)\*:<i>$1</i>:g;
    # [link](url)
    s:\[(.*?)\]\((.*?)\):<a href="$2">$1</a>:g;
    # <img…>
    if (s#(?<=<img src=")https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/##g) { #"
        push @images, $_ foreach (/<img src="(.*?)"/g);
        s#<img[^>]+[^/]\K>#/>#g;
    }
    # Headlines
    if (s:^(#+)\s*(.*?)\s*$:"<h".(length $1).">$2</h".(length $1).">":e or /^$/) {
        $intro .= para($paragraph);
        $paragraph = '';
        $intro .= $_;
        next;
    }
    $paragraph .= $_;
}
$intro .= para($paragraph);

# Define the playoreder/sequnce.
# Introduction first, then the puzzles in alphabetical order
my @content = ($TITLE, sort keys %games);
$games{$TITLE} = $intro;


# Write the mimetype file
my $z = IO::Compress::Zip->new( "build/$TITLE.epub",
    AutoClose => 1,
    Name => 'mimetype',
    Method => ZIP_CM_STORE,
    -Level => Z_NO_COMPRESSION,
) or die "zip failed: $ZipError\n";
$z->print('application/epub+zip');

$z->newStream(
    Name => 'META-INF/container.xml',
        Method => ZIP_CM_DEFLATE,
        -Level => Z_BEST_COMPRESSION,
);
$z->print(container_xml());

$z->newStream(
    Name => 'content.opf',
        Method => ZIP_CM_DEFLATE,
        -Level => Z_BEST_COMPRESSION,
);
$z->print(content_opf(\@content, \@images));

$z->newStream(
    Name => 'content.ncx',
        Method => ZIP_CM_DEFLATE,
        -Level => Z_BEST_COMPRESSION,
);
$z->print(content_ncx(\@content));

$z->newStream(
    Name => 'nav.xhtml',
        Method => ZIP_CM_DEFLATE,
        -Level => Z_BEST_COMPRESSION,
);
$z->print(nav_xhtml(\@content));

while (my($title, $text) = each %games) {
    (my $fname = $title) =~ tr/ /_/;
    $z->newStream(
        Name => "$fname.xhtml",
        Method => ZIP_CM_DEFLATE,
        -Level => Z_BEST_COMPRESSION,
    );
    $z->print(chapter_xhtml($title, $text));
}

foreach (@images) {
    open my $input, '<', $_;
    $z->newStream(
        Name => $_,
        Method => ZIP_CM_STORE,
        -Level => Z_NO_COMPRESSION,
    );
    $z->print(<$input>);
    close $input
}

$z->close();

sub container_xml {
    return <<CONTAINERXML;
<?xml version="1.0" encoding="UTF-8"?>
<container
  xmlns="urn:oasis:names:tc:opendocument:xmlns:container"
  version="1.0">
  <rootfiles>
    <rootfile
      full-path="content.opf"
      media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
CONTAINERXML
}

sub content_opf {
    my ($games, $images) = @_;
    my $items;
    my $itemrefs;
    my $sep = '';
    foreach (@$games) {
        (my $fname = $_) =~ tr/ /_/;
        $items .= qq#$sep<item id="$fname" href="$fname.xhtml" media-type="application/xhtml+xml"/>#;
        $itemrefs .= qq#$sep<itemref idref="$fname"/>#;
        $sep = "\n    ";
    }
    foreach (@$images) {
        (my $fname = $_) =~ s#^.*/##;
        $fname =~ tr/ ./__/;
        my $mimetype = "application/text";
        my $extension = lc($1) if /\.(.+)$/;
        SWITCH: for ($extension) {
        /^png$/ && do { $mimetype = "image/png"; last SWITCH; };
        /^gif$/ && do { $mimetype = "image/gif"; last SWITCH; };
        /^jpe?g$/ && do { $mimetype = "image/jpeg"; last SWITCH; };
        }
        $items .= qq#$sep<item id="$fname" href="$_" media-type="$mimetype"/>#;
        $sep = "\n    ";
    }
    return <<CONTENTOPF;
<?xml version="1.0" encoding="UTF-8"?>
<package version="3.0"
  xmlns:dc="http://purl.org/dc/elements/1.1/"
  xmlns:opf="http://www.idpf.org/2007/opf"
  xmlns="http://www.idpf.org/2007/opf"
  unique-identifier="Id">
  <metadata>
    <dc:identifier id="Id">$UUID</dc:identifier>
    <meta property="dcterms:modified">$DATE</meta>
    <dc:language>en</dc:language>
    <dc:title xml:lang="de">$TITLE</dc:title>

    <dc:description xml:lang="de">$DESCRIPTION</dc:description>
    <dc:creator id="author_pocket_puzzles" xml:lang="de">$AUTHOR</dc:creator>
    <meta refines="#author_pocket_puzzles" property="file-as">$AUTHOR_SORT</meta>
    <meta refines="#author_pocket_puzzles" property="role" scheme="marc:relators">aut</meta>
    <meta property="identifier-type" refines="#Id">UUID</meta>
    <meta property="dcterms:created">$DATE</meta>
    <meta property="dcterms:issued">$DATE</meta>
  </metadata>

  <manifest>
    <item id="ncx"     href="content.ncx"   media-type="application/x-dtbncx+xml"/>
    <item id="nav"     href="nav.xhtml"    media-type="application/xhtml+xml" properties="nav"/>
    $items
  </manifest>
  <spine toc="ncx">
    <itemref idref="nav"/>
    $itemrefs
  </spine>
</package>
CONTENTOPF
}

sub content_ncx {
    my ($games) = @_;
    my $navpoints;
    my $playorder = 0;
    foreach (@$games) {
        (my $fname = $_) =~ tr/ /_/;
        ++$playorder;
        $navpoints .= <<NAVPOINT;
    <navPoint playOrder="$playorder" id="id_$playorder">
      <navLabel>
        <text>$_</text>
      </navLabel>
      <content src="$fname.xhtml"/>
    </navPoint>
NAVPOINT
    }
    chomp($navpoints);
    return<<CONTENTNCX;
<?xml version="1.0" encoding="UTF-8"?>
<ncx
  xmlns="http://www.daisy.org/z3986/2005/ncx/"
  version="2005-1"
  xml:lang="de">
  <head>
    <meta name="dtb:uid" content="$UUID"/>
  </head>
  <docTitle>
    <text>$TITLE</text>
  </docTitle>
  <docAuthor>
    <text>$AUTHOR</text>
  </docAuthor>
  <navMap>
    $navpoints
  </navMap>
</ncx>
CONTENTNCX
}

sub nav_xhtml {
    my ($games) = @_;
    my $elements;
    foreach (@$games) {
        (my $fname = $_) =~ tr/ /_/;
        $elements .= <<ELEMENT;
      <li><a href="$fname.xhtml">$_</a></li>
ELEMENT
    }
    chomp($elements);
    return <<NAVXHTML;
<?xml version="1.0" encoding="UTF-8" ?>
<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:ops="http://www.idpf.org/2007/ops"
      xml:lang="de">
  <head>
    <title>Table of content</title>
  </head>
  <body>
   <nav ops:type="toc">

    <h1>Table of content</h1>

    <ol>
$elements
    </ol>

  </nav>
 </body>
</html>
NAVXHTML
}

sub chapter_xhtml {
    my ($title, $text) = @_;
    return <<CONTENTXHTML;
<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml"
      xmlns:ops="http://www.idpf.org/2007/ops"
      xml:lang="de">
  <head>
    <title>$title</title>
  </head>
  <body>
    <h1>$title</h1>
$text
  </body>
</html>
CONTENTXHTML
}

sub para {
    local($_) = @_;
    return '' unless $_;
    if (/\A(^[-*].*$)+\Z/s) {
        s:^[-*]\s*(.*?)\s*$:<li>$1</li>:gm;
        return "<ul>\n$_\n</ul>\n";
    }
    if (/\A(^\d+\..*$)+\Z/s) {
        s:^\d+\.\s*(.*?)\s*$:<li>$1</li>:gm;
        return "<ol>\n$_\n</ol>\n";
    }
    return "<p>\n$_</p>\n";
}