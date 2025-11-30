# Localise MagiC root fs
# Note that this script can be run from anywhere
# TODO: should run recursively

if [ "$#" -eq 1 ]; then
    cd "$(dirname "$0")"
    CODE=$1
    # overwrite all programs and kernel
    cp -pf $CODE/MAGICLIN.OS ../
    cp -pf $CODE/GEMSYS/GEMDESK/*.RSC ../GEMSYS/GEMDESK/
    cp -pf $CODE/GEMSYS/GEMDESK/*.PRG ../GEMSYS/GEMDESK/
    cp -pf $CODE/GEMSYS/GEMDESK/*.TXT ../GEMSYS/GEMDESK/
    # do not overwrite application database, if exists
    cp -p --update=none $CODE/GEMSYS/GEMDESK/APPLICAT.DAT ../GEMSYS/GEMDESK/
    cp -p --update=none $CODE/GEMSYS/GEMDESK/APPLICAT.INF ../GEMSYS/GEMDESK/
else
    echo "usage: LOCALISE.SH DE|EN|FR"
fi
