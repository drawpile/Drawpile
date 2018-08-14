unzip drawpile.zip
cd Drawpile-*/
mkdir build
cd build
cmake .. -DCLIENT=OFF -DSERVER=ON -DSERVERGUI=OFF -DCMAKE_BUILD_TYPE=Release
make
mv bin/drawpile-srv /build/

