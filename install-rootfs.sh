# If you change the rootfs path, you must later adapt the config file accordingly!
ROOTFS=`xdg-user-dir DOCUMENTS`/MAGIC_C
# This is only the default language, it can easily be changed later.
INITIAL_LANGUAGE=EN

echo "create Atari root filesystem C: in $ROOTFS  ..."
mkdir -p $ROOTFS
cp -rpn rootfs/* $ROOTFS/
echo "localise Atari root filesystem to $INITIAL_LANGUAGE ..."
touch $ROOTFS/MAGICLIN.OS
$ROOTFS/LANG/LOCALISE.SH $INITIAL_LANGUAGE
