## ToDo

### Milestones for alpha version

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

### Milestones for beta version

- [X] Implement custom game parameters entry screen
- [X] Implement help screen
- [X] Better color palette for greyscale screen (done for all active games)
- [X] Better error visualization suitable for greyscale screen (done for all active games)
- [X] Modify games with dragging for better handling of eInk screen limitations (done for all active games)
- [X] B/W texture drawing for games depending on color output (done for all active games)

### For future versions

- [X] Ability to mark games as favorite
- [X] Entry buttons indicating status (Bridges, Dominosa, games with one-click fill)
- [ ] Context menu for chooser buttons (Launch, Resume, Clear prefs, Set/Unset as favorite)
- [ ] Check draw update efficiency in all games
- [ ] Check all games for memory leaks
- [ ] General settings (show/hide statusbar, orientation, chooser style, color/grey mode)
- [ ] Configurable game specific UI settings (short/long click action, helper buttons, classic/one-click entry)
- [ ] Information screen explaining game controls
- [ ] Color mode
- [ ] Screen orientation handling
- [ ] Generate separate documentation eBook

## ToDo for individual games

- [X] Ascent: Add number keys; better error display, allow number erase by backspace
- [X] Boats: Change error for incomplete boats, adjacent boats, and wrong boat numbers
- [X] Dominosa: Error coloring, thicker line
- [X] Dominosa: Number highlighting 
- [X] Filling: Error coloring
- [X] Keen: Persist pencil marker, error circle, error background for clues
- [X] Loopy: Add dotted line draw for line errors, diagonal dotted line, better presets
- [X] Magnets: Better presets
- [X] Pattern: Black-white-neutral circling on click
- [X] Pattern: Better utilization of screen space
- [X] Pearl: Colors
- [X] Range: Background error & outline error for black squares
- [X] Salad: Colors
- [X] Salad: No 'O' in letters mode
- [X] Singles: Show mark on adjacent black squares error, error background on white squares
- [X] Solo: Lighter activation pencil mark, better presets
- [X] Spokes: Better error display
- [X] Towers: Persist pencil marker
- [X] Tracks: Background color on circular error
- [X] Undead: Ghost lines, Different monster colors, fix ghost polygon error
- [X] Undead: Stronger monster outlines
- [X] Tents: Change error for tree without tent
- [X] Palisade: Thicker lines / dotted lines on error / clue error background
- [X] Palisade: Fill finished regions
- [X] Unequal: Coloring, <> polygon error, pencil marker persistence, adjacent error display, error circles, presets with Kropki mode
- [X] Unruly: Coloring
- [X] Bridges: Add path button
- [X] Bridges: Adjust game colors
- [X] Guess: Change blitter drag to Highlight color by click -> Fill peg by click
- [X] Guess: Add color fill pattern
- [X] Group: Error display
- [X] Untangle: Change drag to Highlight vertex by click -> update after drag to target coordinates
- [X] Cube: Remove animation. Adjust colors.
- [X] Cube: Thicker lines for cube
- [X] Flip: Remove animation. Adjust colors.
- [X] Fifteen: Remove animation. Adjust colors. Add solver hint mechanism.
- [X] Flood: Add color fill pattern
- [X] Inertia: Remove animation
- [X] Inertia: Adjust colors, thicker grid lines, modify gem icons
- [X] Net: Remove animation. Adjust colors.
- [X] Net: Add shuffle button
- [X] Pegs: Remove blitter drag
- [X] Rome: Change to highlight / highlight clue; add up/down/left/right buttons
- [X] Signpost: Remove blitter drag. Adjust colors. Modify left/right click behavior. Fix polygon error.
- [X] Sixteen: Remove animation
- [X] Twiddle: Remove animation
- [X] Dominosa: Number highlighting in numerical buttons
- [X] Loopy: Reverse whole cell on hint number error
- [X] Map: Remove drag animation
- [X] Map: Color fill pattern
- [X] Samegame: Add color fill pattern
- [X] Signpost: Rework cell arrow highlighting to avoid whole screen redraw
- [X] Solo: Number highlighting indication in numerical buttons
- [X] Loopy: Check min/max grid sizes
- [X] Bridges: Change Path button to indicator
- [X] Tracks: Rework UI (cell versus border clicks)
- [X] Creek: Separate from Slant as standalone game
- [X] Walls: Expand error display
- [X] Walls: Expand solver / difficulty levels (area parity criteria)
- [ ] Galaxies: Rework right-drag / arrow handling
- [ ] Loopy: Fix polygon error fill in Penrose grids
- [ ] Mathrax: Deactivate useless 'recursive' game difficulty
- [ ] Map: Optimize redraw of area hint marks
- [ ] Mosaic: Undo of whole drag sequence
- [ ] Netslide: Remove animation. Thicker lines.
- [ ] Pearl: Better error display for error line
- [ ] Pearl: "No line" on right click instead of release
- [ ] Slant: 'Hard' game difficulty (backtracking)
- [ ] Sticks: Implement dragging
- [ ] Twiddle: Re-activate basic animation (?)
- [ ] Walls: Add dragging

#### One-click symbol fill

- [X] Solo
- [X] Towers
- [X] Keen
- [X] Mathrax
- [X] ABCD
- [X] Rome
- [X] Salad
- [X] Undead
- [X] Unequal
- [X] Group
- [X] Filling

#### Rework Border error highlighting

- [ ] Boats
- [ ] Pattern
- [ ] Tents
- [X] Undead

#### Grey-out of finished hints

- [X] ABCD
- [X] Bricks
- [X] Boats
- [ ] Pattern
- [X] Range
- [ ] Salad (in ABC End View Mode)
- [X] Tents
- [X] Tracks

#### Game specific UI settings

- [ ] Bricks: Short-click, black or white
- [ ] Bridges: 'Show Grid' button
- [ ] Clusters: Short-click, black or white
- [ ] Creek: Short-click, black or white
- [ ] Map: Pencil fill, all or possible only
- [ ] Mines: Short-click, flag or reveal
- [ ] Mosaic: Short-click, black or white
- [ ] Pattern: Short-click, black or white
- [ ] Range: Short-click, block or empty
- [ ] Singles: Short-click, black or circle
- [ ] Signpost: Long-click: Show incoming arrows or not
- [ ] Unruly: Short-click, black or white
- [ ] Games with one-click: on/off
- [ ] Games with 'Fill pencil marks' button
- [ ] Games with Pencil mark indicator: Flip on swap button

