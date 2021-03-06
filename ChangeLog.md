## ChangeLog

## 0.7.9 nightly

### Added
* *Filling*: One-click fill
* *Filling*: Undo of cell highlighting during dragging

### Changed
* Increase space between menu buttons
* *Galaxies*: Disallow placing an edge touching a dot (sync with upstream)
* *Guess*: Minor colour tweaks for better contrast

### Fixed
* Horizontal center placement of chooser page indicator
* *Filling*: Show only allowed number buttons

## 0.7.8 - 2021/05/16

### Changed
* Sync with upstream (Centralize initial clearing of puzzle window)
* *Map*: Hint button places only possible colours.
* *Guess*: Minor UX tweaks
* *Magnets*: Adjust size requirements for tricky puzzles to avoid game generation hangs

### Fixed
* Game frontend tried to access non-existent swap key in *Ascent* and *Signpost*, causing crashes

## 0.7.7 - 2021/04/23

### Added
* *Loopy*: New grid type *compass dodecagonal*

### Changed
* *Filling*: Sync with upstream (better randomization in puzzle generation, bugfixes)
* Deactivated puzzles *Group* and *Sokoban*, deemed too esoteric (Group) or generator yet too simple (Sokoban)

### Fixed
* Bug of disappearing statusbar on manually triggered screen redraw
* Removed unneccessary double screen updates on manual refresh

## 0.7.6 - 2021/03/12

### Fixed
* Crash when launching any game from chooser, due to sneaky memory corruption bug.

## 0.7.5 - 2021/02/23

### Changed
* *Signpost*: Coloring of start / end cells in a sequence.

### Fixed
* *Creek*: Bug that disconnected grid still counted as valid solution.

## 0.7.4 - 2021/02/17

### Added
* Swiping in chooser to switch pages
* *Undead*: Show number of placed monster types

### Changed
* Chooser page indicator / select buttons
* UX handling in *Tracks* puzzle partially reverted.

## 0.7.3 - 2021/02/08

### Changed
* Reworked border size in *Map*, *Dominosa*, *Galaxies*, *Inertia*, *Mines*, *Net*, *Rect*, *Sokoban*, *Walls* for better screen utilization
* Reorganized icons folder

### Fixed
* *Undead*: Fix incorrect redraw on 'Show solution'

## 0.7.2 - 2021/01/10

### Added
* *Sticks* puzzle
* Drag mechanism in *Creek* puzzle
* Mention individual game contributors in respective "How to play" infoboxes

### Changed
* Extract *Creek* into separate puzzle
* UX handling in *Tracks* puzzle
* Calculate chooser button layout and screen fontsizes according to screen resolution

### Fixed
* Statusbar update
* Reduce screen redraw in *Spokes* and *Bridges* puzzles

## 0.7.1 - 2020/12/30

### Added
* "Hint greyout" in *ABCD*, *Boats*, *Tents*

### Fixed
* "Hint greyout" when short/long button swap is active

## 0.7.0 - 2020/12/27

### Added
* "One-click fill" in *Group*, *Salad*, *Rome*, *Undead*, *Towers*, *Mathrax*, *ABCD*, *Unequal*
* Indicator status control buttons

### Fixed
* Difficulty levels in *Kropki* variation in *Unequal* puzzle

## 0.6.3 - 2020/12/15

### Added
* *SameGame* puzzle
* *Map* puzzle

### Changed
* Reduce amount of screen redraw in *Signpost* puzzle
* Finer textures in *Flood* puzzle
* Textures in *SameGame* puzzle
* UX in *Map* puzzle

### Fixed
* Save game state on EVT_EXIT (device shutdown etc.)

## 0.6.0 - 2020/12/01

### Added
* Mark games as favorite in game chooser

## Beta - 2020/11/14
