wget https://github.com/KDE/extra-cmake-modules/archive/v5.49.0.zip -O ecm.zip
wget https://github.com/KDE/karchive/archive/v5.49.0.zip -O karchive.zip

unzip ecm.zip
unzip karchive.zip

cd /build/extra-cmake-modules-*
cmake .
make install

cd /build/karchive-*
mkdir build
cd build
cmake ..
make
make install

