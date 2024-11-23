
On an debian/ubuntu derived OS:

```
# clone the repository
cd ~/repos/
git clone --recurse-submodules https://github.com/RetroTournaments/argos.git

# install necessary packages
sudo apt-get install                        \
    build-essential cmake ninja-build       \
    libzmq3-dev pkg-config libgtk-3-dev libopencv-dev libasound2-dev


# Build the main executable
cd ~/repos/argos
mkdir build && cd build
cmake -G Ninja ..
ninja
```

