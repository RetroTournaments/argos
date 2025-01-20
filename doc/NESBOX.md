NESBox README
-------------

We need a more efficient way to setup, transport, and use Static and host STA events.
Therefore we will use the time-tested approach of 'putting things in a box.'

The NESBox should contain _all_ relevant hardware in a safe and contained environment.
It must protect the hardware from damage, be secure during transport and setup.
It should be easy to setup, and easy to develop, construct and debug.
It serves a dual purpose as a stand for the CRTs.

First major decision is to have one box for two setups.
I _believe_ that one small NUC should be enough for that and it should reduce costs pretty substantially.

Connections:
- Input Power via NEMA C14
- In/Out Ethernet for network control
- Output Power for two CRT televisions NEMA 5-15 receptacle
- Output Composite video / audio for two CRT televisions
- Output Visual information to the speedrunners

Inside
- Power distribution
- Two modified nintendo entertainment systems
- Computer
- Hardware to split video signal from NES to computer (for capture), and to back panel for the CRTs
- Hardware to support the mod.
- Hardware for two independent video capture

Additional thoughts
- Support two Sony PVM 20M4U systems, 66 lb, roughly 16 x 18 inches
- Ventilation?
- Handles for carrying?


NESBox computer setup
---------------------

[Download ARCH iso](https://archlinux.org/download/) from a mirror and make a [usb flash install drive](https://wiki.archlinux.org/title/USB_flash_installation_medium).

Unfortunately I had to use [etcher](https://etcher.balena.io/) which was kind of annoying. Just using `sudo -i ; cat blah.iso > /dev/usb... ; sync` wasn't being seen on the windows box.

Go through the inane setup of the pc with windows (might need to select work / school / and domain sync to avoid microsoft account garbage). Then install arch via the usb overwriting everything.

On my beelink thing I have to spam f7 during setup, go to boot, and set the order to usb then it worked.

## Conect to wifi during setup
```
> iwctl
> device list
> station wlan0 scan
> station wlan0 get-networks
> station wlan0 connect _________
```

## Arch install
Then `archinstall`

- Set hostname something like `nesbox01`
- set root password. I'm setting all of mine to the same because YOLO
- Set user account > add user w/ password as sudoer
- Audio: pulseaudio
- additional packages, base base-devel openssh neovim ffmpeg git usbutils alsa-utils sof-firmware alsa-ucm-conf tmux cmake
- Network configuration > Use NetworkManager
- Timezone MDT

Disk configuration, best effort looked good. btrfs seemed fine. May investigate again later.

### Network setup
After launching set up network:


```
nmcli dev wifi connect NETGEAR36 password "password_here"
nmcli device # to get name
nmcli device set wlo1 autoconnect yes
```

## Now setup ssh access both ways
```
sudo pacman -S openssh neovim
sudo ssh-keygen -A
sudo systemctl start sshd
sudo systemctl enable sshd
```

On the machine:
```
ip addr #find the 192.168.1.22
```

Then from the mothership:
```
ssh-copy-id matthew@192.168.1.22
``

And setup `~/.ssh/config` on the mothership

```
Host nesbox01
    User matthew
    HostName 192.168.1.23
```

and on the box

```
Host mothership
    User matthew
    HostName 192.168.1.15
```

Now I can just turn it on and then ssh in no worries :D


## Get video / audio capture sorted on the nesbox
```
pacman -S ffmpeg git usbutils alsa-utils sof-firmware alsa-ucm-conf
git clone https://github.com/RetroTournaments/static.git
```

```
sudo usermod -a -G video,audio,uucp matthew
```

On this setup I need the following

```
arecord -l

v4l2-ctl --list-formats-ext
```

Then the following ffmpeg command seems ok:
```
ffmpeg -f v4l2 -thread_queue_size 2048 -input_format mjpeg -framerate 60 -video_size 1024x768 -i /dev/video0 -f alsa -thread_queue_size 2048 -i sysdefault:CARD=MS2109 out2.mkv
# or
ffmpeg -f v4l2 -thread_queue_size 2048 -i /dev/video0 -f alsa -thread_queue_size 2048 -i sysdefault:CARD=MS210x out3.mkv
```

Or this
```
ffmpeg -r 30 -f v4l2 -thread_queue_size 2048 -i /dev/video0 -f alsa -thread_queue_size 2048 -i sysdefault:CARD=MS210x -preset ultrafast -vcodec libx264 -tune zerolatency -b 900k -f mpegts udp://192.168.1.15:12345 
```

Also need to look into the segmenter!

Then on the mothership

```
ffplay -fflags nobuffer -flags low_delay -framedrop -strict experimental -vf setpts=0 udp://192.168.1.15:12345
```


To log in automatically
```
sudo vim /etc/systemd/getty.target.wants/getty@tty1.service
# then edit execstart to '-a' auto login matthew
ExecStart=-/sbin/agetty -a matthew - $TERM
```

To launch tmux on startup, but I didn't really like this. Commented out at the moment.
```
sudo pacman -S tmux
```

Then set up the .bashrc which includes the snippet to start tmux / attach to the 'static' tmux session

## x/ graphics

```
lspci -v | grep -A1 -e VGA -e 3D

sudo pacman -S mesa xorg xorg-xinit xorg-xeyes xterm
sudo pacman -S xfce4 xfce4-goodies xorg xorg-server lightdm-gtk-greeter-settings lightdm-gtk-greeter
sudo systemctl enable lightdm
reboot
```

## Ugh

I need to collect this all into one big startup script.

```
sudo pacman -S cmake

mkdir 3rd
cd 3rd
git clone https://github.com/libsdl-org/SDL.git
cd SDL
mkdir build
cd build
cmake ..
sudo make install
```

```
cd ~/3rd/
git clone https://github.com/libsdl-org/SDL_mixer
cd SDL_mixer
mkdir build && cd build
cmake ..
sudo make install
```

```
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git

cd opencv
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE \
    -D CMAKE_INSTALL_PREFIX=/usr/local \
    -D OPENCV_GENERATE_PKGCONFIG=ON \
    -D OPENCV_EXTRA_MODULES_PATH=~/3rd/opencv_contrib/modules \
    -D BUILD_EXAMPLES=OFF ..

make -j8
sudo make install

```

Also for sdl_mixer and cmake.. Eventually those should be um. pinned to a release or something. Not sure.


```
pacman -S gtk3
```

## Usb mapping
So the tool of choice here seems to be dmesg. Which must be run as root. On the little nuc I'm using now:
I found these by plugging things in / out.

```
 front                        back
+-------------------------+  +-------------------------+
|                         |  |  ____                   | 
| ____   ____         __  |  | |____|1-4               | 
||____| |____|       |__| |  |  ____                   | 
| 1-2    1-1          pwr |  | |____|1-3               | 
|                         |  |                         | 
+-------------------------+  +-------------------------+ 
```

So then we can parse dmesg with this information so long as we are consistent in plugging in left / right.
Or I suppose I could swap them via some config after checking.


git clone --recurse-submodules matthew@nesrack:/home/matthew/repos/static
