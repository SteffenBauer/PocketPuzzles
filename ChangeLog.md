## ChangeLog

## 0.7.11 nightly

### Changed
* All games: Use status bar to show game completion instead of pushy message box
* *Net*: Add lock button for locking tiles in place where one is sure of the orientation.
* *CrossNum*: Adjust level difficulty labels
* *Solo*: Finish number entry in manual mode now by clicking + or - button

### Fixed
* *Rome*: Fixed missing screen update on button click
* *Ascent*: Screen update of number buttons
* *Unruly*: Memory leak
* *Solo* and *Unequal*: Bug that made it possible to overwrite given fixed numbers

## 0.7.10 - 2022/04/02

### Added
* *Walls*: Place paths / walls by dragging (swipe across cells for paths, along edges for walls)
* *Spokes*: Additional difficulty level
* *CrossNum* puzzle

### Changed
* *Solo*: Removed some presets, for better fit in the limited screen size
* *Walls*: Better error display (diagonal stripes)
* Cleaner screen layout by removing unnecessary 'force screen redraw' button from chooser and param screens
* Optimized screen update in game screen
* Games with pencil marks now arrange them in numerically fixed position.

### Fixed
* *Mosaic*: Fix preservation of 'aggressiveness' parameter
* *Solo*: Disallow certain corner-case parameters to avoid hang on game generation

## 0.7.9 - 2021/10/26

### Added
* *Bricks* puzzle
* *Filling*: Drag already placed or fixed number to fill empty cells
* *Filling*: Undo of cell highlighting during dragging
* *Solo*: Manual mode; for transferring puzzles from newspapers to play them on the app
* *Unruly*: Add additional difficulty level (sync with upstream)
* *Unruly*: Show option "unique rows/columns" in preset strings
* *Sticks*: Input dragging

### Changed
* Sync with upstream (use 'aux' solver in *Galaxies*, fix tent placement in *Tents*)
* Increase space between menu buttons
* 'Show solution' menu entry only in games supporting solve operation
* *Galaxies*: Disallow placing an edge touching a dot (sync with upstream)
* *Galaxies*: Remove debug code
* *Guess*: Minor colour tweaks for better contrast
* *Ascent*, *Filling*, *Mathrax*, *Salad*, *Solo*, *Towers*, *Unequal*: Paint fixed numbers in bold font; user input numbers in normal font
* *Ascent*, *Filling*, *Mathrax*, *Salad*, *Solo*, *Towers*, *Unequal*: Errors in stronger contrast
* *Walls*: Complete rewrite. More advanced & faster generator; better UI

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
