#!/bin/bash
# prerequisites:
#  git clone https://gitlab.com/AndreasK/magiclinux
#  cd magiclinux

echo "install system libraries for graphics, messages and sound..."
case `uname -s` in
Linux*)
    os=linux
    release=`lsb_release -i -s 2>/dev/null | tr '[[:upper:]]' '[[:lower:]]'`
    case "$release" in
    ubuntu | debian)
        sudo apt install libsdl2-dev libsdl2-mixer-dev gxmessage
        ;;
    opensuse | leap)
        sudo zypper install SDL2-devel SDL2_mixer-devel cmake pkgconf-pkg-config
        # TODO: gxmessage not a package in openSUSE, but can be compiled from source
        ;;
    fedora)
        sudo yum install SDL2-devel SDL2_mixer-devel cmake
        # TODO: gxmessage not a package in Fedora, but can be compiled from source
        ;;
    *)
        echo "Don't know how to install packages on $release; assuming already installed" >&2
        ;;
    esac
Darwin*)
    os=macos
    if test "`which port 2>/dev/null`" != ""; then
        sudo port install cmake pkgconfig libsdl2 libsdl2_mixer
    elif test "`which brew 2>/dev/null`" != ""; then
        sudo brew install sdl2 sdl2_mixer pkg-config cmake
    else
        echo "Don't know how to install packages on macOS" >&2
        echo "Please install MacPorts or HomeBrew first" >&2
        exit 1
    fi
    ;;
    esac
Haiku*)
    os=haiku
    pkgman install cmake libsdl2_devel sdl2_mixer_devel
    # TODO: gxmessage not a package in Haiku, but can be compiled from source
    ;;
MINGW*|MINGW*|MSYS*)
    os=win32
    echo "Windows is currently not supported" >&2
    exit 1
    ;;
CYGWIN*)
    os=cygwin
    echo "Cygwin is currently not supported" >&2
    exit 1
    ;;
*)
    echo "unknown operating system" >&2
    exit 1
    ;;
esac
echo "build emulator executable..."
mkdir -p build
pushd build >/dev/null
# VARIANTS: cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX:PATH=/opt ..
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
make
echo "register emulator with MIME types and icon..."
sudo make install
popd
case $os in
linux)
    echo "register MIME type and icon for Atari executables ..."
    sudo ./install-program-mime-types.sh
    sudo ./install-cartridge-mime-types.sh
    if test "`which hatari 2>/dev/null`" != ""; then
        echo "Hatari is installed. Please read instructions in ./install-image-mime-types.sh!"
    else
        echo "Hatari is not installed; register MIME type and icon for Atari ST disk images ..."
        sudo ./install-image-mime-types.sh
    fi
    ;;
esac
./install-rootfs.sh
