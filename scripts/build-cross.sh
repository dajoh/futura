#!/usr/bin/env bash

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# -------------------------------------------
# Set up a dir in ~ to use for compilation
# -------------------------------------------
pushd ~
mkdir cross
cd cross

# -------------------------------------------
# Update system packages
# -------------------------------------------
echo ""
echo "[!] Updating system packages..."
sleep 1

sudo apt-get update
sudo apt-get upgrade

# -------------------------------------------
# Install build prerequisites
# -------------------------------------------
echo ""
echo "[!] Installing build prerequisites..."
sleep 1

sudo apt-get install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo

# -------------------------------------------
# Download binutils and gcc sources
# -------------------------------------------
echo ""
echo "[!] Downloading binutils and gcc sources..."
sleep 1

wget https://ftp.gnu.org/gnu/binutils/binutils-2.34.tar.gz
wget https://ftp.gnu.org/gnu/gcc/gcc-9.3.0/gcc-9.3.0.tar.gz

# -------------------------------------------
# Extract binutils and gcc sources
# -------------------------------------------
echo ""
echo "[!] Extracting binutils and gcc sources..."
sleep 1

tar -xzf binutils-2.34.tar.gz
tar -xzf gcc-9.3.0.tar.gz

# -------------------------------------------
# Build binutils
# -------------------------------------------
echo ""
echo "[!] Building and installing binutils..."
sleep 1

mkdir build-binutils
cd build-binutils
../binutils-2.34/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j 8
make install
cd ..

# -------------------------------------------
# Build gcc
# -------------------------------------------
echo ""
echo "[!] Building and installing gcc..."
sleep 1

mkdir build-gcc
cd build-gcc
../gcc-9.3.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-bootstrap
make -j 8 all-gcc
make -j 8 all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

# -------------------------------------------
# Print success message and restore CWD
# -------------------------------------------
echo ""
echo "!!! SUCCESS !!!"
popd
