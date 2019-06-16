# chunkwm float

Plugin to help with better floating window management in chunkwm.

See [issue #1](https://github.com/Amar1729/chunkwm-float/issues/1) for planned/work-in-progress features.

## Installation

I may set up a tap soon, but for now only installation is from source.
Clone this repo, `make -f src/makefile`, and copy the resulting plugin under `bin/` into chunkwm plugin directory.

## Usage

Make sure to add `chunkc core::load float.so` to `~/..chunkwmrc` !

Command-line usage

This plugin supports two modes: `window` and `query`.

```bash
$ chunkc float::window

--inc|-i <north|south|east|west>
--dec|-d <north|south|east|west>
  Increase/Decrease a floating window in the direction given.
  Step size is controlled by "float_resize_step" config, or temporarily by --step

--move|-m <north|south|east|west>
  Move a floating window in the direction given.
  Step size is controlled by "float_move_step" config, or temporarily by --step

--absolute|-a '%fx%f:%fx%f'
  Resize a floating window to the given coordinates.
  %f is a float between 0 and 1.
  Order is:
  X_initial Y_initial X_final Y_final
    e.g. 0.0x0.0:0.5x1.0 fills entire left half of screen)

--step|-s %f
  Temporary step size ratio for --move|--inc|--dec.
  %f is a float between 0 and 1.
  %f represents a ratio of the size of the screen
    e.g. --step 0.1 --move east will move a window 1/10 of the screen rightward
  note - --step must be passed on cli first!


$ chunkc float::query

--position|-p <x|y|w|h>
  Get the pixel measurement of given parameter of focused window.
```

### Configuration

This plugin supports the following configurations (in `~/.chunkwmrc`) :

- `float_move_step` : 
  - as a fraction of the screen (0 to 1) how much a floating window will move (default 0.05)

- `float_resize_step` : 
  - as a fraction of the screen (0 to 1) how much a floating window will dilate (default 0.025)

- `global_float_offset_top` : 
  - margin at top of screen (default 5px)

- `global_float_offset_bottom` : 
  - margin at bottom of screen (default 5px)

- `global_float_offset_left` : 
  - margin at left of screen (default 5px)

- `global_float_offset_right` : 
  - margin at right of screen (default 5px)


