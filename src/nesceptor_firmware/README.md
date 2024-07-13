
The sdk documentation: https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf


To get picotool, normally into ~/3rd/
```
git clone https://github.com/raspberrypi/picotool.git
cd picotool
sudo apt install build-essential pkg-config libusb-1.0-0-dev cmake
sudo cp udev/99-picotool.rules /etc/udev/rules.d/

mkdir build
cd build

cmake -DPICO_SDK_PATH=~/repos/argos/3rd/pico-sdk/ ..
make
sudo make install

```
