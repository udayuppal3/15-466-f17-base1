NOTE: please fill in the first section with information about your game.

# Cave Explorer

Cave Explorer is Uday Uppal's implementation of [Cave Explorer](http://graphics.cs.cmu.edu/courses/15-466-f17/game1-designs/aluo/) for game1 in 15-466-f17.

![Image](screenshots/Game1ScreenShot4.png?raw=true "Image")

## Asset Pipeline

The assets were built using Vectr, a simple vector graphics program. Assets included textures for background grid pieces, the player, rocks, treasure, and text phrases. All assets were generated inside one file. This file was then exported to a png format, and inserted into the game. The different assets were then loaded from the texture atlas based on their placement and size. Many assets were drawn rotated and resized to save space.

## Architecture

The code is divided into different sections. There is a section for sprite loading that handles the loading of the various assets from the texture atlas. There is also a section for information about the pathing on the map, since this is a static map and there are certain rules that must be followed by the player (so they cannot float through walls). Within the game loop, there is also a drawing section where first the background cells are drawn, then the rocks, players, treasure, and text. 

## Reflection

The main difficulty in the assignment was generating the map and pathing rules, since this was a very specific map and had to be implemented by hand (a 6x5 grid). This could probably have been done in some clever way programatically, but I just wanted to get something working. The texture loading and rendering worked surprisingly well, since I made sure to generate a texture atlas that had nice, easy to query sizes and positions for all textures. If I were doing this again, I would find a better way to bake in the pathing and drawing rules for the grid of cells.

The design document was very clear on the map and gameplay mechanics, including how to move the player and how to explore the map. It was a little ambigous about what cells to display after exploration, and also the end state of the game. I decided that I would display only the cells that the player has explored so far, and that the game would end as soon as the treasure was found, making the player immobile.

# About Base1

This game is based on Base1, starter code for game1 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
