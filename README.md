# :godmode: Godmode9 :godmode:
_A full access file browser for the 3DS console_

GodMode9 is a full access file browser for the Nintendo 3DS console, giving you access to your SD card and to the FAT partitons inside your SysNAND and EmuNAND. Among other functionality (see below), you can copy, delete, rename files and create folders. A write permission system prevents you from doing dangerous stuff without noticing and forces you to enter unlock sequences for confirmation.


## Warning
__This is powerful stuff__, but precautions are taken so you don't accidentially damage the filesystem of your console. The write permissions system protects you by providing warnings and forces you to enter an unlock sequence for enabling write permissions. It is not possible to overwrite or modify any important stuff without such unlock sequences and it is not possible to accidentially unlock something.

After unlocking write permissions on SysNAND, any writing change to your CTRNAND can result in a brick (still recoverable via A9LH). Writing changes to your TWLN partition can lead to a partial brick, leading to your console not being able to run DSiWare / DS cartridges anymore. If you unlock the A9LH write permission, you are able to overwrite and remove the A9LH installation in your system, which allows you to full brick your console (in that case, only recoverable via hardmod).

__As always, be smart, keep backups, just to be safe__.


## What you can do with GodMode9
With the possibilites GodMode9 provides not everything may be obvious at first glance. So, for your convenience a (incomplete!) list of what GodMode9 can do follows below.
* __Manage files on all your data storages__: You wouldn't have expected this, right? Included are all standard file operations such as copy, delete, rename files and create folders. Use the L button to mark multiple files and apply file operations to multiple files at once.
* __Make screenshots__: Press R+L anywhere. Screenshots are in BMP format.
* __Use multiple panes__: Press R+left|right. This enables you to stay in one location in the first pane and open another in the second pane.
* __Search drives and folders__: Just press R+A on the drive / folder you want to search.
* __Format your SD card / setup a RedNAND__: Press the HOME button, select `SD format menu`. This also allows to setup a RedNAND on your SD card. You will get a warning prompt and an unlock sequence before any operation starts.
* __Run it without an SD card / unmount the SD card__: If no SD card is found, you will be offered to run without the SD card. You can also unmount and remount your SD card from the file system root at any point.
* __Direct access to SD installed contents__: Just take a look inside the `A:`/`B:` drives. One the fly crypto is taken care for, you can access this the same as any other content.
* __Build CIAs from NCCH / NCSD (.3DS) / TMD (installed contents)__: Press A on the file you want converted, the option will be shown. Installed contents are found (among others) in `1:/titles/`(SysNAND) and `A:/titles/`(SD installed). Where applicable, you will also be able to generate legit CIAs. Note: this works also from a file search.
* __Decrypt and verify NCCH / NCSD / CIA / TMD / FIRM images__: Options are found inside the A button menu. You will be able to decrypt to the standard output directory or (where applicable) in place.
* __Batch mode for the above two operations__: Just select multiple files of the same type via the L button, then press the A button on one of the selected files.
* __Access any file inside NCCH / NCSD / CIA / FIRM images__: Just mount the file via the A button menu and browse to the file you want.
* __Generate XORpads for any NAND partition__: Take a look inside the `X:` drive. You can use these XORpads for decryption of encrypted NAND images on PC. Additional tools such as [3dsFAT16Tool](https://github.com/d0k3/3DSFAT16tool/releases) are required on PC.
* __Directly mount and access NAND dumps or standard FAT images__: Just press the A button on these files to get the option. You can only mount NAND dumps from the same console.
* __Restore / dump NAND partitions or even full NANDs__: Just take a look into the `S:` (or `E:`/ `I:`) drive. This is done the same as any other file operation.
* __Compare and verify files__: Press the A button on the first file, select `Calculate SHA-256`. Do the same for the second file. If the two files are identical, you will get a message about them being identical. On the SDCARD drive (`0:`) you can also write a SHA file, so you can check for any modifications at a later point.
* __Hexview and hexedit any file__: Press the A button on a file and select `Show in Hexeditor`. A button again enables edit mode, hold the A button and press arrow buttons to edit bytes. You will get an additional confirmation prompt to take over changes. Take note that for certain file, write permissions can't be enabled.
* __Inject a file to another file__: Put exactly one file (the file to be injected from) into the clipboard (via the Y button). Press A on the file to be injected to. There will be an option to inject the first file into it.


## How to run this / entry points
GodMode9 can be built to run from a number of entry points, descriptions are below. Note that you need to be on or below 3DS firmware version v9.2 or have ARM9loaderhax installed for any of these to work. All entrypoint files are included in the release archive.
* __A9LH & Brahma__: Copy `GodMode9.bin` to somewhere on your SD card and run it via either [Brahma](https://github.com/delebile/Brahma2) or [arm9loaderhax](https://github.com/Plailect/Guide/wiki). Brahma derivatives / loaders (such as [BrahmaLoader](https://gbatemp.net/threads/release-easily-load-payloads-in-hb-launcher-via-brahma-2-mod.402857/), [BootCTR](https://gbatemp.net/threads/re-release-bootctr-a-simple-boot-manager-for-3ds.401630/) and [CTR Boot Manager](https://gbatemp.net/threads/ctrbootmanager-3ds-boot-manager-loader-homemenuhax.398383/)) and A9LH chainloaders (such as [Luma3DS](https://github.com/AuroraWright/Luma3DS) and [BootCTR9](https://github.com/hartmannaf/BootCtr9)) will work with this as well. Build this with `make a9lh`.
* __Homebrew Launcher__: Copy `GodMode9.3dsx` & `GodMode9.smdh` into `/3DS/GodMode9` on your SD card. Run this via [Smealums Homebrew Launcher](http://smealum.github.io/3ds/), [Mashers Grid Launcher](https://gbatemp.net/threads/release-homebrew-launcher-with-grid-layout.397527/) or any other compatible software. Build this with `make brahma`.
* __CakeHax Browser__: Copy `GodMode9.dat` to the root of your SD card. You can then run it via http://dukesrg.github.io/?GodMode9.dat from your 3DS browser. Build this via `make cakehax`.
* __CakeHax MSET__: Copy `GodMode9.dat` to the root of your SD card and `GodMode9.nds` to anywhere on the SD card. You can then run it either via MSET and GodMode9.nds. Build this via `make cakerop`.
* __Gateway Browser Exploit__: Copy Launcher.dat to your SD card root and run this via http://go.gateway-3ds.com/ from your 3DS browser. Build this with `make gateway`. Please note: __this entrypoint is deprecated__. While it may still work at the present time with little to no problems, bugs will no more be fixed and it may be completely removed at a later time. Use CakeHax instead.

If you are a developer and you are building this, you may also just run `make release` to build all files at once. To build __SafeMode9__ (a bricksafe variant of GodMode9, with smaller functionality) instead of GodMode9, compile with `make MODE=safe`. For additional customization, you may also choose the internal font via `make FONT=6X10`, `make FONT=ACORN`, `make FONT=GB` and `make FONT=ORIG`.


## Support files
For certain functionality, GodMode9 may need 'support files'. Support files can be placed into either `0:/`(the SD root folder), `0:/files9/` or `1:/rw/files9/` (all locations will work). Support files contain additional information that is required in decryption operations. A list of support files, and what they do, is found below. Please don't ask for support files - find them yourself.
* __`aeskeydb.bin`__: This should contain 0x25keyX, 0x18keyX and 0x1BkeyX to enable decryption of 7x / Secure3 / Secure4 encrypted NCCH files and 0x11key95 / 0x11key96 for FIRM decrypt support. As an alternative (not recommended), legacy `slot0x??key?.bin` files are also supported. It can be created from your existing `slot0x??key?.bin`files in Decrypt9 via the 'Build Key Database' feature.
* __`seeddb.bin`__: This file is required to decrypt and mount seed encrypted NCCHs and CIAs if the seed in question is not installed to your NAND. Note that your seeddb.bin must also contain the seed for the specific game you need to decrypt.
* __`otp.bin`__: This file is console-unique and is required - on entrypoints other than A9LH - for decryption of the 'secret' sector 0x96 on N3DS (and O3DS with a9lh installed). Refer to [this guide](https://github.com/Plailect/Guide/wiki) for instructions on how to get your own `otp.bin` file.
* __`sector0x96.bin`__ / __`secret_sector.bin`__ : A copy of the decrypted, untouched (non-a9lh) secret sector. This is required for decryption of the encrypted ARM9 section of N3DS FIRMs. It is not required for anything else. As an alternative you can also provide the required keys inside your aeskeydb.bin.
* __`encTitleKeys.bin`__ / __`decTitleKeys.bin`__: These files are optional and provide titlekeys, which are required to create updatable CIAs from NCCH / NCSD files. CIAs created without these files will still work, but won't be updatable from eShop.


## License
You may use this under the terms of the GNU General Public License GPL v2 or under the terms of any later revisions of the GPL. Refer to the provided `LICENSE.txt` file for further information.

## Credits
This tool would not have been possible without the help of numerous people. Thanks go to...
* **Archshift**, for providing the base project infrastructure
* **Normmatt**, for sdmmc.c / sdmmc.h
* **Cha(N)**, **Kane49**, and all other FatFS contributors for FatFS
* **SciResM** for helping me figure out RomFS 
* **b1l1s** for helping me figure out A9LH compatibility
* **Gelex** and **AuroraWright** for helping me figure out various things
* **dark_samus** for the new 6x10 font and help on various things
* **Al3x_10m**, **Supster131**, **imanoob**, **Kasher_CS** and all other fearless testers
* The fine folks on **freenode #Cakey**
* Everyone I possibly forgot, if you think you deserve to be mentioned, just contact me!