# PocketPuzzles

![PocketPuzzles Chooser](https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/screenshots/puzzles_chooser.png)

### About

Port of [Simon Tatham's Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/) to the [PocketBook eReader family](https://de.wikipedia.org/wiki/PocketBook)

**Work in Progress**. First commit, only GUI framework functional, no actual puzzles implemented so far. See ToDo below.

Tested only on the **PocketBook Touch HD 3**, because that is the device I own. Lots of hacking and reverse-engineering, as documentation for PocketBook app development is very, very sparse.

I'm planning to also include unofficial puzzles and puzzle variations from [puzzles-unreleased](https://github.com/x-sheep/puzzles-unreleased) and [sgtpuzzles-extended](https://github.com/SteffenBauer/sgtpuzzles-extended)

### Note

Build and installation is completely **on your own risk**! You must be aware that this software could brick your device.

See also [LICENSE](https://raw.githubusercontent.com/SteffenBauer/PocketPuzzles/master/LICENSE)

### ToDo

- [X] Basic GUI framework
- [X] Generate game bitmaps (game icons, menu buttons, control buttons)
- [X] Game chooser
- [X] Game screen menubar
- [X] Game screen statusbar
- [X] Game presets
- [ ] Drawing callback functions
- [ ] Game initialization
- [ ] Game launch
- [ ] Implement game menu controls (new, restart, solve)
- [ ] Implement swap / undo / redo functionality
- [ ] Setup of game specific control buttons
- [ ] Setup of game specific type presets
- [ ] Save / restore of game state on app exit / relaunch
- [ ] Save / restore current game preset on game screen / app exit / relaunch
- [ ] Implement custom game type entry screen
- [ ] Implement help screen
- [ ] Ability to mark games as favorite
- [ ] Screen orientation handling

