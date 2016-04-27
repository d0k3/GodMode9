# :godmode: Godmode9 :godmode:
_A full access file browser for the 3DS console_

GodMode9 is a full access file browser for the Nintendo 3DS console, giving you access to your SD card and to the FAT partitons inside your SysNAND and EmuNAND. As of now, you can copy, delete, rename files and create folders. A write permission system prevents you from doing dangerous stuff without noticing.

## Warning
__This is powerful stuff__, so you should not use it if you don't know exactly what you're doing. This tool won't protect you from yourself. On SysNAND, any writing change to your CTRNAND can result in a full brick. Writing changes to your TWLN partition can lead to a partial brick, leading to your console not being able to run DSiWare / DS cartridges anymore. Always keep backups, just to be safe.

## How to run this / entry points

GodMode9 can be built to run from a number of entry points, descriptions are below. Note that you need to be on or below 3DS firmware version v9.2 or have ARM9loaderhax installed for any of these to work.
* __A9LH, Brahma & Bootstrap__: Copy `GodMode9.bin` to somewhere on your SD card and run it via either [Brahma](https://github.com/delebile/Brahma2) or [Bootstrap](https://github.com/shinyquagsire23/bootstrap). Brahma derivatives / loaders such as [BrahmaLoader](https://gbatemp.net/threads/release-easily-load-payloads-in-hb-launcher-via-brahma-2-mod.402857/), [BootCTR](https://gbatemp.net/threads/re-release-bootctr-a-simple-boot-manager-for-3ds.401630/) and [CTR Boot Manager](https://gbatemp.net/threads/ctrbootmanager-3ds-boot-manager-loader-homemenuhax.398383/) will also work with this. Build this with `make bootstrap`.
* __Homebrew Launcher__: Copy `GodMode9.3dsx` & `GodMode9.smdh` into `/3DS/GodMode9` on your SD card. Run this via [Smealums Homebrew Launcher](http://smealum.github.io/3ds/), [Mashers Grid Launcher](https://gbatemp.net/threads/release-homebrew-launcher-with-grid-layout.397527/) or any other compatible software. Build this with `make brahma`.
* __CakeHax Browser__: Copy `GodMode9.dat` to the root of your SD card. You can then run it via http://dukesrg.github.io/?GodMode9.dat from your 3DS browser. Build this via `make cakehax`.
* __CakeHax MSET__: Copy `GodMode9.dat` to the root of your SD card and `GodMode9.nds` to anywhere on the SD card. You can then run it either via MSET and GodMode9.nds. Build this via `make cakerop`.
* __Gateway Browser Exploit__: Copy Launcher.dat to your SD card root and run this via http://go.gateway-3ds.com/ from your 3DS browser. Build this with `make gateway`. Please note: __this entrypoint is deprecated__. While it may still work at the present time with little to no problems, bugs will no more be fixed and it may be completely removed at a later time. Use CakeHax instead.

If you are a developer and you are building this, you may also just run `make release` to build all files at once. If you are a user, all files are already included in the release archive.

## Credits
This tool would not have been possible without the help of numerous people. Thanks go to...
* **Archshift**, for providing the bas eproject infrastructure
* **Normmatt**, for sdmmc.c / sdmmc.h
* **Cha(N)**, **Kane49**, and all other FatFS contributors for FatFS
* **b1l1s**, for helping me figure out A9LH compatibility
* **Al3x_10m**, **Supster131** and all other fearless testers
* The fine folks on **freenode #Cakey**
* Everyone I possibly forgot, if you think you deserve to be mentioned, just contact me!