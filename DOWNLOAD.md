## Pre-compiled app binaries

You can find pre-compiled binaries ready for installation here.

* [V0.8.2 (latest stable)](https://github.com/SteffenBauer/PocketPuzzles/releases/download/0.8.2/SGTPuzzles.app.zip)
* [V0.8.1](https://github.com/SteffenBauer/PocketPuzzles/releases/download/0.8.1/SGTPuzzles.app.zip)
* [V0.8.0](https://github.com/SteffenBauer/PocketPuzzles/releases/download/0.8.0/SGTPuzzles.app.zip)
* [V0.7.11](https://github.com/SteffenBauer/PocketPuzzles/releases/download/0.7.11/SGTPuzzles.app.zip)
* [V0.7.10](https://github.com/SteffenBauer/PocketPuzzles/releases/download/0.7.10/SGTPuzzles.app.zip)
* [V0.7.9](https://github.com/SteffenBauer/PocketPuzzles/releases/download/0.7.9/SGTPuzzles.app.zip)

### Installation

Download `SGTPuzzles.app.zip` from one of the above links. Unzip the file. Connect your PocketBook to your PC with an USB cable. Copy `SGTPuzzles.app` to the folder `/applications` on your device. Unmount the USB connection, and reboot the PocketBook. You should now see an entry `@SGTPuzzles` among your apps.

### Nightly build

[Nightly build](https://github.com/SteffenBauer/PocketPuzzles/releases/download/2025092801/SGTPuzzles.app.zip)

The nightly pre-release will be kept in sync with the current master branch. Install this if you want the newest features; keep in mind that this build might be unstable and less thoroughly tested.

### Upgrading

**Important notes on upgrading the app**

#### From version lower than 0.7.12
In the nightly release and upcoming stable releases (0.7.12 and higher) the *loopy* puzzle will not contain all grid types anymore. The app may crash when trying to launch a *loopy* game configured to one of the unsupported grid types.

To prevent a crash, configure the *loopy* game to a *squares* grid type before upgrading.

#### Devices with firmware 5

Version 0.8.2 was compiled with the current SDK 6.8 to support dark mode. This included a check that the device is running with firmware 6.

As this prevented older devices that are still running firmware 5 from running the app, I made the app backwards-compatible with firmware 5. So if you have a device with firmware 5, please install any version **other than 0.8.2**.

### Important Note!

Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

