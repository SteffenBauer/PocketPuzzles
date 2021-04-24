# PocketPuzzles

Port of [Simon Tatham's Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/) to the [PocketBook eReader family](https://de.wikipedia.org/wiki/PocketBook)

<img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_chooser_stars.png" width="220"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_sologame.png" width="220"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_gameconfig.png" width="220">

[More screenshots](https://github.com/SteffenBauer/PocketPuzzles/blob/master/screenshots/screenshots.md)

Inspired by and building on the work of [Port to Android by Chris Boyle](https://github.com/chrisboyle/sgtpuzzles) and [Puzzles for pocketbook by mnk](https://github.com/svn2github/pocketbook-free/tree/master/puzzles)

To get a mostly complete collection for all puzzles ever written for the SGT puzzles, I'm planning to also include unofficial puzzles and puzzle variations from [puzzles-unreleased](https://github.com/x-sheep/puzzles-unreleased) and [sgtpuzzles-extended](https://github.com/SteffenBauer/sgtpuzzles-extended), and the *Mosaic* puzzle from [here](https://github.com/kohend/simon-puzzles)

### Development history

See [ChangeLog.md](https://github.com/SteffenBauer/PocketPuzzles/blob/master/ChangeLog.md)

**14.11.2020 Beta version ready!** Games can now be individually configured. All elements needed for beta are now implemented.

**6.10.2020 Alpha version ready!** Persistence of game params and current game now working. Most games UI are reworked suitable for eInk screen.

**16.8.2020 Next milestone** Game control buttons now working. All games playable (but those using blitter / animations still need rewriting)

**11.8.2020 First major milestone reached!** Some games are now playable on a very basic level. 

There is still work to do. See ToDo list below. eInk screens are limited in response time and color availability, so most of the games need individual tweaking to make them fun to play.

See [ToDo.md](https://github.com/SteffenBauer/PocketPuzzles/blob/master/ToDo.md)

### Binary app

Connect your PocketBook with you PC with an USB cable. Copy `release/SGTPuzzles.app` to the folder `/applications` on your device. Unmount the USB connection, and reboot the PocketBook. You should now see an entry `@SGTPuzzles` among your apps.

If you want the newest possible version build from the latest commit, use `release/nightly/SGTPuzzles.app`.

### Build

Clone the [pocketbook SDK](https://github.com/blchinezu/pocketbook-sdk/) and set it up for the firmware of your device.

Modify the path to your local copy of the SDK in the `Makefile` at the line `PBSDK ?= ...`

Run: `make`

When everything compiles successfully, you find `build/SGTPuzzles.app`. Copy this to the folder `/applications` on your device. Reboot if necessary after transfer.

### Compatibility

Tested only on the **PocketBook Touch HD 3**, because that is the device I own. Should principally work on all devices with firmware 5 or higher.

I received reports that the app works with these devices:

* PB632  - Touch HD 3
* PB631  - Touch HD / Touch HD 2
* PB740  - InkPad 3
* PB633  - Pocketbook Color
* PB1040 - InkPad X
* PB626  - Touch Lux 3 (see [#12](/../../issues/12) for possible issue, but should be fixed now)

I'm interested in reports from more devices; especially how it looks like on color screen and on devices with screen resolution different than the Touch HD 3.

### MIT License

See [LICENSE](https://github.com/SteffenBauer/PocketPuzzles/blob/master/LICENSE)

This project is using work released unter a MIT license by Simon Tatham, Chris Boyle, Richard Boulton, James Harvey, Mike Pinna, Jonas Kölker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd Schmidt, Steffen Bauer, Lennard Sprong, Rogier Goossens, Michael Quevillon, Phil Tunstall, and Didi Kohen.

### Important Note!

Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

