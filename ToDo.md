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
- [X] Configurable game specific UI settings (short/long click action, helper buttons, classic/one-click entry)
- [ ] Information screen explaining game controls
- [ ] Color mode
- [X] Dark mode support
- [ ] Screen orientation handling
- [ ] Generate separate documentation eBook
- [ ] App icon

### Current bugs

- [ ] Undo button still activated after change of game parameters (bug in upstream)

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
- [X] Sticks: Implement dragging
- [X] Filling: Drag already placed/fixed numbers
- [X] Walls: Add dragging
- [X] Net: Add "Lock" button
- [X] Untangle: Hint mechanism
- [ ] Rome: Dragging
- [ ] Galaxies: Rework right-drag / arrow handling
- [ ] Mathrax: Deactivate useless 'recursive' game difficulty
- [ ] Map: Optimize redraw of area hint marks
- [ ] Mosaic: Undo of whole drag sequence
- [ ] Netslide: Activate game; Remove animation. Thicker lines.
- [ ] Pearl: Better error display for error line
- [ ] Pearl: "No line" on right click instead of release
- [ ] Slant: 'Hard' game difficulty (backtracking)
- [ ] Twiddle: Re-activate basic animation (?)
- [X] Dominosa: Remove limit on highlighted numbers
- [X] Creek: Smaller border
- [X] Slant: Smaller border
- [ ] Pattern: Optimize border size calculation
- [X] Bridges: Change current hint finish indicator to grey-out

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

#### Rework Border error highlighting

- [ ] Boats
- [ ] Pattern
- [ ] Tents
- [X] Undead

#### Grey-out of finished hints

- [X] ABCD
- [X] Bricks
- [X] Bridges: Change current hint finish indicator to grey-out
- [X] Boats
- [ ] Pattern
- [X] Range
- [ ] Salad (in ABC End View Mode)
- [X] Tents
- [X] Tracks

#### Fixed grid arrangement of pencil marks

- [X] ABCD
- [X] CrossNum
- [X] Keen
- [X] Mathrax
- [X] Salad
- [X] Solo
- [X] Towers
- [X] Undead
- [X] Unequal

#### Game specific UI settings

- [X] Bricks: Short-click, black or white
- [X] Bridges: 'Show Grid' button
- [X] Clusters: Short-click, black or white
- [X] Creek: Short-click, black or white
- [X] Map: Show pencil button; Pencil fill, all or possible only
- [X] Mines: Short-click, flag or reveal
- [ ] Mosaic: Short-click, black or white
- [ ] Pattern: Short-click, black or white
- [X] Range: Short-click, block or empty; Show Hint button
- [X] Signpost: Long-click, Show incoming arrows or not
- [X] Singles: Short-click, black or circle; show black numbers
- [X] Slant: Show errors; show filled cells; Short-click, left-upper or right-upper line
- [X] Unruly: Show errors; Short-click, black or white
- [ ] Games with one-click: on/off
- [ ] Games with 'Fill pencil marks' button: visible/hidden
- [ ] Games with Pencil mark indicator: Flip on swap button
- [ ] Games with pencil marks: Fixed / Floating arrangement

#### Dark mode specific UI tweaks

- [ ] Binary: Border & tiles 3D effect
- [ ] Bricks: Cell colors
- [ ] Clusters: Cell colors
- [ ] Creek: Cell colors
- [ ] Fifteen: Border & tiles 3D effect
- [ ] Inertia: 3D effect of blocked cells
- [ ] Mines: Border & Cells 3D effect
- [ ] Mosaic: Cell colors
- [ ] Pattern: Cell colors
- [ ] Pearl: Hint colors
- [ ] SameGame: Border 3D effect
- [ ] Sixteen: Border & tiles 3D effect
- [ ] Twiddle: Border & tiles 3D effect
- [ ] Undead: Monster icons
- [ ] Unequal/Kropki mode: Hint colors
- [ ] Unruly: Cell colors; 3D effect of fixed cells

