# Raingrasp

<p align="center">
  <img src="logo.png" alt="Raingrasp Logo" />
</p>

A C++ recreation of the two *The Elder Scrolls Travels* feature phone games, **Stormhold** and **Dawnstar**. It aims for an authentic experience by default, but with some optional modernizations. This fixes any bugs I noticed in the original.

This does not include any game assets. You will need to have your own copy of the original mobile game `.jar` files.

## Modernizations

This uses controls designed for a keyboard, not a phone pad. Additional shortcuts have been added to reduce on the amount of menuing.

There is also a widescreen mode. This widescreen mode also switches to using 3D projects of the walls instead of the hand-authored ones in the original. This is for a couple reasons. First, the perspective in the base game is confusing. Second, the existing art does not cover the additional required views. This is unfortunate since the hand-authored perspectives look considerably better than the 3D view.

Help text is not updated with the more modern controls.

## Running the game

The game will prompt you for jar files on boot. Drag them onto the window to use them. You only need to do this once per game.

This launches directly into the last game you played. You can switch games with 'Game Select' on the menu.

Save data and jar unpacks are written to `%LOCALAPPDATA%\Raingrasp\`.

From the repository, build and launch the game with:

```
bazelisk run //:raingrasp
```

## Building

This project uses Bazel to build and test. This is untested on anything other than Windows.

It uses MSVC as the compiler.

```
bazelisk build //:dist
bazelisk test //:test_fast --enable_runfiles
bazelisk test //:test_quick --enable_runfiles
bazelisk test //:test_frames --enable_runfiles
```

Build for release with the following command.

```
bazelisk build //:dist
powershell -Command "Compress-Archive -Path bazel-bin/raingrasp -DestinationPath raingrasp.zip -Force"
```

### Running playthrough tests

To run the more extensive tests, you need  the jar files. Place `tes-travels-dawnstar-1.0.0.jar` and `tes-travels-stormhold-1.0.10.jar` in `artifacts/private/jars/`.

## Disclaimer

Raingrasp is an unofficial, fan-made project. It is not affiliated with,
authorized by, endorsed by, or in any way associated with Bethesda Softworks
LLC, ZeniMax Media Inc., Vir2L Studios, or any of their subsidiaries or
affiliates. *The Elder Scrolls*, *Stormhold*, *Dawnstar*, and all related
names and marks are trademarks of their respective owners, used here only to
identify the games this project recreates.

This project ships no game assets. Running it requires your own legally
obtained copies of the original `.jar` files.

## License

Copyright (C) 2026 Peakt.

Raingrasp is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [`COPYING`](COPYING) for the complete license and
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for the separately licensed
SDL and stb components.
