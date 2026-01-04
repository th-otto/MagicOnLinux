# Localise MagiC root fs
# Note that this script can be run from anywhere.
# Note that macOS only knows "-v" and "-n" and not "--verbose" and "--update=none".
# TODO: should run recursively


#VERBOSE="-v"
VERBOSE=""

if [ "$#" -eq 1 ]; then
    # go to directory where the script resides
    cd "$(dirname "$0")"
    # convert language code to uppercase
    CODE=`echo $1 | tr a-z A-Z`
    # check if new kernel file exists
    if ! test -f $CODE/MAGICLIN.OS; then
        echo "LOCALISE.SH: Valid country codes are: "; ls -d ??
        exit 1
    fi
    # check if kernel file already matches
    if ! cmp --quiet $CODE/MAGICLIN.OS ../MAGICLIN.OS; then
        # overwrite all programs and kernel
        cp $VERBOSE -pf $CODE/MAGICLIN.OS ../
        cp $VERBOSE -pf $CODE/GEMSYS/GEMDESK/*.RSC ../GEMSYS/GEMDESK/
        cp $VERBOSE -pf $CODE/GEMSYS/GEMDESK/*.PRG ../GEMSYS/GEMDESK/
        cp $VERBOSE -pf $CODE/GEMSYS/GEMDESK/*.TXT ../GEMSYS/GEMDESK/
        cp $VERBOSE -pf $CODE/GEMSYS/GEMDESK/*.TOS ../GEMSYS/GEMDESK/
        # do not overwrite application database, if exists
        cp $VERBOSE -pn $CODE/GEMSYS/GEMDESK/APPLICAT.DAT ../GEMSYS/GEMDESK/ 2>/dev/null
        cp $VERBOSE -pn $CODE/GEMSYS/GEMDESK/APPLICAT.INF ../GEMSYS/GEMDESK/
    fi
else
    echo "usage: LOCALISE.SH DE|EN|FR"
    #echo $#
    exit 1
fi
