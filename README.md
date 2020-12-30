# PocketPuzzles

Port of [Simon Tatham's Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/) to the [PocketBook eReader family](https://de.wikipedia.org/wiki/PocketBook)

<img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_chooser_stars.png" width="220"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_sologame.png" width="220"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_gameconfig.png" width="220">

[More screenshots](https://github.com/SteffenBauer/PocketPuzzles/blob/master/screenshots/screenshots.md)

Inspired by and building on the work of [Port to Android by Chris Boyle](https://github.com/chrisboyle/sgtpuzzles) and [Puzzles for pocketbook by mnk](https://github.com/svn2github/pocketbook-free/tree/master/puzzles)

To get a mostly complete collection for all puzzles ever written for the SGT puzzles, I'm planning to also include unofficial puzzles and puzzle variations from [puzzles-unreleased](https://github.com/x-sheep/puzzles-unreleased) and [sgtpuzzles-extended](https://github.com/SteffenBauer/sgtpuzzles-extended), the unofficial *Group*, *Sokoban*, and *Slide* puzzles from the original collection, and the *Mosaic* puzzle from [here](https://github.com/kohend/simon-puzzles)

### Development history

**30.12.2020 v0.7.1** Implemented 'hint grey-out'. In the puzzles with this feature, one can now click on a game hint to grey it out. This serves the purpose of marking a hint as 'done', so it doesn't distract anymore from puzzle solving. See ToDo.md for list of affected games.

**27.12.2020 v0.7.0** Implemented 'one-click symbol fill' entry method in games that use numbers/letters/symbols in a grid (see ToDo.md for the list of affected games). Usage: When no grid cell is currently highlighted or 'pencil mark highlighted', one can press a control button. That button is then highlighted until pressed again or another button is highlighted. While a button is selected, clicking into a cell directly enters that symbol; long-click enters a pencil mark.

**15.12.2020 v0.6.3** Added 'Map' and 'Samegame'; switched both games from color to grayscale textures.

**01.12.2020 v0.6.0** One can now mark games as favorite on the chooser screen by long-pressing the game icon.

**14.11.2020 Beta version ready!** Games can now be individually configured. All elements needed for beta are now implemented.

**6.10.2020 Alpha version ready!** Persistence of game params and current game now working. Most games UI are reworked suitable for eInk screen.

**16.8.2020 Next milestone** Game control buttons now working. All games playable (but those using blitter / animations still need rewriting)

**11.8.2020 First major milestone reached!** Some games are now playable on a very basic level. 

There is still work to do. See ToDo list below. eInk screens are limited in response time and color availability, so most of the games need individual tweaking to make them fun to play.

### Binary app

Connect your PocketBook with you PC with an USB cable. Copy `release/SGTPuzzles.app` to the folder `/applications` on your device. Unmount the USB connection, and reboot the PocketBook. You should now see an entry `@SGTPuzzles` among your apps.

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
* PB626  - Touch Lux 3 (with minor issues so far, see [#12](/../../issues/12))

I'm interested in reports from more devices; especially how it looks like on color screen and on devices with screen resolution different than the Touch HD 3.

### ToDo

#### Milestones for alpha version

- [X] Basic GUI framework
- [X] Generate game bitmaps (game icons, menu buttons, control buttons)
- [X] Game chooser
- [X] Game screen menubar
- [X] Game screen statusbar
- [X] Game presets
- [X] Drawing callback functions
- [X] Game initialization
- [X] Game launch
- [X] 'Game solved' announcement
- [X] Screen click handling
- [X] Colors -> Greyscale
- [X] Implement game menu controls (new, restart, solve)
- [X] Implement swap / undo / redo functionality
- [X] Handling of physical device buttons
- [X] Setup of game specific control buttons
- [X] Setup of game specific type presets
- [X] Save / restore game presets on screen change / app exit / relaunch
- [X] Save / restore of current game state on screen change / app exit / relaunch
- [X] 'About' box

#### Milestones for beta version

- [X] Implement custom game parameters entry screen
- [X] Implement help screen
- [X] Better color palette for greyscale screen (done for all active games)
- [X] Better error visualization suitable for greyscale screen (done for all active games)
- [X] Modify games with dragging for better handling of eInk screen limitations (done for all active games)
- [X] B/W texture drawing for games depending on color output (done for all active games)

#### For future versions

- [X] Ability to mark games as favorite
- [X] Entry buttons indicating status (Bridges, Dominosa, games with one-click fill)
- [ ] Context menu for chooser buttons (Launch, Resume, Clear prefs, Set/Unset as favorite)
- [ ] Check draw update efficiency in all games
- [ ] Check all games for memory leaks
- [ ] General settings (show/hide statusbar, orientation, chooser style, color/grey mode)
- [ ] Information screen explaining game controls
- [ ] Color mode
- [ ] Screen orientation handling
- [ ] Generate separate documentation eBook

ToDos for individual games see [ToDo.md](https://github.com/SteffenBauer/PocketPuzzles/blob/master/ToDo.md)

### MIT License

See [LICENSE](https://github.com/SteffenBauer/PocketPuzzles/blob/master/LICENSE)

This project is using work released unter a MIT license by Simon Tatham, Chris Boyle, Richard Boulton, James Harvey, Mike Pinna, Jonas Kölker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd Schmidt, Steffen Bauer, Lennard Sprong, Rogier Goossens, Michael Quevillon, Phil Tunstall, and Didi Kohen.

### Important Note!

Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

