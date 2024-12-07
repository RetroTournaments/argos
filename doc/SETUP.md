
On an debian/ubuntu derived OS:


```
# install necessary packages
sudo apt-get install \
    build-essential cmake ninja-build sqlite3 git-lfs \
    libzmq3-dev pkg-config libgtk-3-dev libopencv-dev libasound2-dev

git lfs install

# clone the repository, and necessary data
cd ~/repos/
git clone --recurse-submodules https://github.com/RetroTournaments/argos.git
git lfs pull origin main


# Build the main executable
cd ~/repos/argos
mkdir build && cd build
cmake -G Ninja ..
ninja
```

