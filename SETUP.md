
This markdown file will eventually include all of the instructions to get a fresh clone of argos up and running on a fresh linux install.
But for now it's just a few disorganized commands and notes from earlier.

```
sudo apt-get install build-essential pkg-config libgtk-3-dev \
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev \
    libxvidcore-dev libx264-dev libjpeg-dev libpng-dev libtiff-dev \
    gfortran openexr libatlas-base-dev python3-dev python3-numpy \
    libtbb2 libtbb-dev libdc1394-22-dev libcurl4-openssl-dev libssl-dev \
    libmariadb-dev libarchive-dev libsqlite3-dev mariadb-client mariadb-server \
    v4l-utils

```


```
git clone https://github.com/libsdl-org/SDL.git
cd SDL && mkdir build && cd build
cmake ..
sudo make install
```

```
git clone https://github.com/libsdl-org/SDL_mixer.git
cd SDL_mixer && mkdir build && cd build
cmake -DSDL3MIXER_OPUS=OFF -DSDL3MIXER_MOD=OFF -DSDL3MIXER_MIDI=OFF -DSDL3MIXER_WAVPACK=OFF ..
sudo make install
```

```
mkdir ~/3rdRepos/
cd ~/3rdRepos/

git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git

cd opencv
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE \
    -D CMAKE_INSTALL_PREFIX=/usr/local \
    -D INSTALL_C_EXAMPLES=ON \
    -D INSTALL_PYTHON_EXAMPLES=ON \
    -D OPENCV_GENERATE_PKGCONFIG=ON \
    -D OPENCV_EXTRA_MODULES_PATH=~/3rdRepos/opencv_contrib/modules \
    -D BUILD_EXAMPLES=OFF ..

make -j8
sudo make install
```

```
sudo usermod -a -G dialout $USER
sudo usermod -a -G tty $USER
sudo chmod 666 /dev/ttyUSB*
```

