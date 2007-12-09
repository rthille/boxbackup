#!perl

$basedir = $0;
$basedir =~ s/\\[^\\]*$//;
$basedir =~ s/\\[^\\]*$//;
$basedir =~ s/\\[^\\]*$//;
$basedir =~ s/\\[^\\]*$//;
$basedir =~ s/\\[^\\]*$//;
-d $basedir or die "$basedir: $!";
chdir $basedir or die "$basedir: $!";

require "$basedir\\infrastructure\\BoxPlatform.pm.in";

my $version_string = "#define BOX_VERSION \"$BoxPlatform::product_version\"\n";

open VERSIONFILE, "< $basedir/lib/common/BoxVersion.h";
my $old_version = <VERSIONFILE>;
close VERSIONFILE;

if ($old_version eq $version_string)
{
	print "Version unchanged.\n";
	exit 0;
}

print "New version: $BoxPlatform::product_version\n";

open VERSIONFILE, "> $basedir/lib/common/BoxVersion.h" 
	or die "BoxVersion.h: $!";
print VERSIONFILE "#define BOX_VERSION \"$BoxPlatform::product_version\"\n";
close VERSIONFILE;

exit 0;
