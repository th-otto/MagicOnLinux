# prerequisites:
#  git clone https://gitlab.com/AndreasK/magiclinux
#  cd magiclinux

echo "install system libraries for graphics, messages and sound..."
sudo apt install libsdl2-dev libsdl2-mixer-dev gxmessage
echo "build emulator executable..."
mkdir -p build
pushd build >/dev/null
# VARIANTS: cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX:PATH=/opt ..
cmake -DCMAKE_BUILD_TYPE=Release ..
make
echo "register emulator with MIME types and icon..."
sudo make install
popd
echo "register MIME type and icon for Atari executables ..."
sudo ./install-program-mime-types.sh
if [ -f "/usr/local/share/mime/packages/hatari.xml" ]; then
    echo "Hatari is installed. Please read instructions in ./install-image-mime-types.sh"!
else
    echo "Hatari is not installed; register MIME type and icon for Atari ST disk images ..."
    sudo ./install-image-mime-types.sh
fi
./install-rootfs.sh

