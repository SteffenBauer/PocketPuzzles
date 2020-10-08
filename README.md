# PocketPuzzles

Port of [Simon Tatham's Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/) to the [PocketBook eReader family](https://de.wikipedia.org/wiki/PocketBook)

To get a mostly complete collection for all puzzles ever written for the SGT puzzles, I'm planning to also include unofficial puzzles and puzzle variations from [puzzles-unreleased](https://github.com/x-sheep/puzzles-unreleased) and [sgtpuzzles-extended](https://github.com/SteffenBauer/sgtpuzzles-extended), the unofficial *Group*, *Sokoban*, and *Slide* puzzles from the original collection, and the *Mosaic* puzzle from [here](https://github.com/kohend/simon-puzzles)

Inspired by and building on the work of [Port to Android by Chris Boyle](https://github.com/chrisboyle/sgtpuzzles) and [Puzzles for pocketbook by mnk](https://github.com/svn2github/pocketbook-free/tree/master/puzzles)

**11.8.2020 Major milestone reached!** Some games are now playable on a very basic level. 

**16.8.2020 Next milestone** Game control buttons now working. All games playable (but those using blitter / animations still need rewriting)

**6.10.2020 Alpha version ready!** Persistence of game params and current game now working. Most games UI are reworked suitable for eInk screen.

There is still work to do. See ToDo list below. eInk screens are limited in response time and color availability, so most of the games need individual tweaking to make them fun to play.

### Binary app

Connect your PocketBook with you PC with an USB cable. Copy `build/SGTPuzzles.app` to the folder `/applications` on your device. Unmount the USB connection, and reboot the PocketBook. You should now see an entry `@SGTPuzzles` among your apps.

### Build and Compatibility

Clone the [pocketbook SDK](https://github.com/blchinezu/pocketbook-sdk/) and set it up for the firmware of your device.

Modify the path to your local copy of the SDK in the `Makefile` at the line `PBSDK ?= ...`

Run:  
```
make
```

When everything compiles successfully, you find `build/SGTPuzzles.app`. Copy this to the folder `/applications` on your device. Reboot if necessary after transfer.

Tested only on the **PocketBook Touch HD 3**, because that is the device I own. Could possibly also work on other devices with firmware 5 or higher.

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

- [ ] Implement custom game parameters entry screen
- [X] Implement help screen
- [X] Better color palette for greyscale screen (done for all active games)
- [X] Better error visualization suitable for greyscale screen (done for all active games)
- [X] Modify games with dragging for better handling of eInk screen limitations (done for all active games)
- [X] B/W texture drawing for games depending on color output (done for Flood & Guess, Map & SameGame currently deactivated)

#### For future versions

- [ ] Ability to mark games as favorite
- [ ] General settings (show/hide statusbar, orientation, chooser style)
- [ ] Screen orientation handling
- [ ] Entry buttons indicating status (Bridges, Dominosa, Solo)
- [ ] Generate separate documentation eBook
- [ ] Review left->right->empty handling for games with select / hint select

ToDos for individual games see [ToDo.md](https://github.com/SteffenBauer/PocketPuzzles/blob/master/ToDo.md)

### MIT License

See [LICENSE](https://github.com/SteffenBauer/PocketPuzzles/blob/master/LICENSE)

This project is using work released unter a MIT license by Simon Tatham, Chris Boyle, Richard Boulton, James Harvey, Mike Pinna, Jonas Kölker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd Schmidt, Steffen Bauer, Lennard Sprong, Rogier Goossens, Michael Quevillon, Phil Tunstall, and Didi Kohen.

### Important Note!

Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### Screenshots
---
<img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_chooser_a.png" width="250"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_chooser_b.png" width="250"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_loopygame.png" width="250"><img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_boatsgame.png" width="250"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_undeadgame.png" width="250"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_wallsgame.png" width="250"><img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_gamemenu.png" width="250"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_gamepresets.png" width="250"> <img src="https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_solved.png" width="250">
---
