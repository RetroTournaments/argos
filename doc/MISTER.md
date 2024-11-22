Trying the Mister FPGA following suggestions from Anthony Jacoway and ItsMaximum.

- Purchased [DE10 Nano dev kit](https://www.mouser.com/ProductDetail/Terasic-Technologies/P0496?qs=%2FacZuiyY%252B4ZdDLJqTxdJ5w%3D%3D&countryCode=US&currencyCode=USD) from mouser. $239.12
- Used an old 2GB micro SD card that was lying around.
    - Formatted to FAT and erased for no particular reason
    - DIDN'T WORK! Needed to buy a 64 gb one for like 12 bucks
- Downloaded Mr Fusion v2.9 img, unzipped and used BalenaEtcher to flash to card.
- Connect to cheap hdmi monitor, power, find stupid cheap usb nintendo controller
- Purchased [Micro usb hub](https://www.amazon.com/MakerSpot-Accessories-Charging-Extension-Raspberry/dp/B01JL837X8/)
- Purchased a [wifi dongle thing](https://www.amazon.com/gp/product/B07P5PRK7J/)
- Connected a keyboard and nes controller through retro usb thing

Going to use [update-all](https://github.com/theypsilon/Update_All_MiSTer/releases/tag/latest) at least initially.
Put it in the scripts directory
Then copied ini to mister.ini and set video mode to 8. Will return with more changes if needed.

Ran the wifi script to connect, and then update all. Then I waited.

Ok needed a ram module (else just gray screen!)
Eventually got io board too. Usb power useful. Power button useful. Needed their cable to connect to PVM. Works!

-----

Some important commands:

`echo "load_core /media/fat/bum.mgl" > /dev/MiSTer_cmd` where

```
<mistergamedescription>
    <rbf>_console/NES_20240912</rbf>
    <file delay="2" type="f" index="0" path="TETRIS.nes"/>
</mistergamedescription>
```

Also the config was stored in `/media/fat/config/NES.CFG`
Needed to set snac to controllers and save the config.


Other tabs:
- https://github.com/wizzomafizzo/mrext/blob/main/docs/remote-api.md
- https://misterfpga.org/viewtopic.php?t=6475
- https://github.com/RGarciaLago/MGL_Core_Setnames/tree/main?tab=readme-ov-file
- https://mister-devel.github.io/MkDocs_MiSTer/developer/conf_str/#status-bit-map
- https://misterfpga.org/viewtopic.php?t=524
- https://github.com/MiSTer-devel/Scripts_MiSTer
- https://github.com/mrchrisster/mister-nes-attract/blob/master/NES-AttractMode_on.sh
