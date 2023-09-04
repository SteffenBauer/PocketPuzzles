## Pre-compiled app binaries

You can find pre-compiled binaries ready for installation here.

Choose between one of these build stages:

* [Stable release (0.8.0)](https://github.com/SteffenBauer/PocketPuzzles/blob/master/release/stable/SGTPuzzles.app) Compiled in sync with the latest tagged release. Recommended.
* [Previous release (0.7.11)](https://github.com/SteffenBauer/PocketPuzzles/blob/master/release/previous/SGTPuzzles.app) The app release tag just before the current stable one. Install this when you have problems with the stable release.
* [Nightly build](https://github.com/SteffenBauer/PocketPuzzles/blob/master/release/nightly/SGTPuzzles.app) This binary is kept in sync with the current master branch. Can contain untested code, so install only when you want the newest features.

### Upgrading
**Important note on upgrading the app**

In the nightly release and upcoming stable releases (0.7.12 and higher) the *loopy* puzzle will not contain all grid types anymore. The app may crash when trying to launch a *loopy* game configured to one of the unsupported grid types.

To prevent a crash, configure the *loopy* game to a *squares* grid type before upgrading.

### Installation

Connect your PocketBook to your PC with an USB cable. Copy `SGTPuzzles.app` to the folder `/applications` on your device. Unmount the USB connection, and reboot the PocketBook. You should now see an entry `@SGTPuzzles` among your apps.

### Important Note!

Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

