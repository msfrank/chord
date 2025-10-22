Chord - Distributed execution of Zuri programs
==============================================

Copyright (c) 2024, Michael Frank

Overview
--------

Chord is a collection of software implementing sandboxing and distributed remote
execution of Zuri programs.

Prerequisites
-------------

Chord requires the following dependencies to be installed on the system:
```
CMake version 3.27 or greater:      https://cmake.org
Conan version 2:                    https://conan.io
Tempo:                              https://github.com/msfrank/tempo
Lyric:                              https://github.com/msfrank/lyric
Zuri:                               https://github.com/msfrank/zuri
```

Chord also depends on the package recipes from Timbre and expects that the recipes
are exported into the conan2 package cache.

Quick Start
-----------

1. Navigate to the repository root.
1. Build and install Chord using Conan:
  ```
conan create . --build=missing
  ```

Licensing
---------

The software libraries in this package are licensed under the terms of the BSD
3-clause license. The text of the license is contained in the file `LICENSE.txt`.

The _chord-agent_ software program is licensed separately under the terms of the
Affero GNU Public License version 3.0 (AGPL-3.0) or later. The text of the license
is contained in the file `chord_agent/LICENSE.txt`.

The _chord-machine_ software program is licensed separately under the terms
of the Affero GNU Public License version 3.0 (AGPL-3.0) or later. The text of the
license is contained in the file `chord_machine/LICENSE.txt`.
