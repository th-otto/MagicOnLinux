#!/bin/bash
mv kernel/BUILD/DE/MAGICLIN.OS rootfs/LANG/DE
mv kernel/BUILD/EN/MAGICLIN.OS rootfs/LANG/EN
mv kernel/BUILD/FR/MAGICLIN.OS rootfs/LANG/FR

cp -p rootfs/LANG/DE/MAGICLIN.OS ../MAGIC_C/LANG/DE/
cp -p rootfs/LANG/EN/MAGICLIN.OS ../MAGIC_C/LANG/EN/
cp -p rootfs/LANG/FR/MAGICLIN.OS ../MAGIC_C/LANG/FR/

