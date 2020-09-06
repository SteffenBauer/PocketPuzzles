# PocketPuzzles

![PocketPuzzles Chooser](https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_chooser.png) ![PocketPuzzles Game Screen](https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_game.png)

### About

Port of [Simon Tatham's Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/) to the [PocketBook eReader family](https://de.wikipedia.org/wiki/PocketBook)

I'm planning to also include unofficial puzzles and puzzle variations from [puzzles-unreleased](https://github.com/x-sheep/puzzles-unreleased) and [sgtpuzzles-extended](https://github.com/SteffenBauer/sgtpuzzles-extended), and the two unofficial *Group* and *Sokoban* puzzles from the original collection.

Inspired by and building on [Port to Android by Chris Boyle](https://github.com/chrisboyle/sgtpuzzles) and [Puzzles for pocketbook by mnk](https://github.com/svn2github/pocketbook-free/tree/master/puzzles)

**11.8.2020 Major milestone reached!** Some games are now playable on a very basic level. 

**16.8.2020 Next milestone** Game control buttons now working. All games playable (but those using blitter / animations still need rewriting)

Lots of work still to do. See ToDo list below. eInk screens are limited in response time and color availability, so most of the games need individual tweaking to make them fun to play.

### Build and Compatibility

Clone the [pocketbook SDK](https://github.com/blchinezu/pocketbook-sdk/) and set it up for the firmware of your device.

Modify the path to your local copy of the SDK in the `Makefile` at the line `PBSDK ?= ...`

Run:  
```
mkdir build
make
```

When everything compiles successfully, you find `build/SGTPuzzles.app` in the build directory. Copy this to the folder `/applications` on your device. Reboot if necessary after transfer.

Tested only on the **PocketBook Touch HD 3**, because that is the device I own. Could possibly also work on other devices with firmware 5 or higher.

### ToDo

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
- [X] Setup of game specific control buttons
- [X] Setup of game specific type presets
- [ ] Save / restore of game state on app exit / relaunch
- [ ] Save / restore current game preset on game screen / app exit / relaunch
- [ ] Implement custom game type entry screen
- [ ] Implement help screen
- [ ] Ability to mark games as favorite
- [ ] Screen orientation handling
- [ ] Better color palette for greyscale screen (see ToDo.md)
- [ ] Better error visualization suitable for greyscale screen (see ToDo.md)
- [X] Handling of physical device buttons
- [ ] Modify games with dragging for better handling of eInk screen limitations (see ToDo.md)
- [ ] Implement left->right->empty cycle on left-click for all games

### MIT License

See [LICENSE](https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/LICENSE)

This project is using work released unter a MIT license by Simon Tatham, Chris Boyle, Richard Boulton, James Harvey, Mike Pinna, Jonas Kölker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd Schmidt, Steffen Bauer, Lennard Sprong, Rogier Goossens, Michael Quevillon, and Phil Tunstall.

### Important Note!

Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

