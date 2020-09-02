PLEASE NOTE THAT THE GITHUB VERSION OF SLiM IS PRERELEASE SOFTWARE THAT IS UNDER ACTIVE DEVELOPMENT!
---------------------------------------------------------------------------------------------
It is strongly recommended that all end users of SLiM use the current release version, available at [http://messerlab.org/slim/](http://messerlab.org/slim/).
---------------------------------------------------------------------------------------------
 

License
----------

Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.

SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with SLiM.  If not, see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).


Development & Feedback
-----------------------------------
SLiM is under active development, and our goal is to make it as broadly useful as possible.  If you have feedback or feature requests, or if you are interested in contributing to SLiM, please contact Philipp Messer at [messer@cornell.edu](mailto:messer@cornell.edu). Please note that Philipp is also looking for graduate students and postdocs.


Installation from source
----------------------------------

We use CMake, with an out-of-source build, as described here:

https://gitlab.kitware.com/cmake/community/wikis/FAQ#out-of-source-build-trees

CMake is available with `port install cmake` or `brew install cmake` (on OS X, with MacPorts or Homebrew respectively), or with `aptitude install cmake` (on debian).

To compile the development version from this repository, do:

	git clone https://github.com/MesserLab/SLiM.git
	mkdir SLiM_build
	cd SLiM_build
	cmake -DCMAKE_BUILD_TYPE=Release ../SLiM
	make

(You might do `make -j 10` or some such to speed up the build process by building with ten threads simultaneously.)  The resulting binaries (`slim` and `eidos`) will be located in the `SLiM_build` directory.  You can name the build directory anything you wish; there is nothing magical about the name `SLiM_build`.  Running `make install` will then attempt to install these to the default system directories, but this will probably fail unless you run it with root permissions (e.g., by doing `sudo make install`).   To explicitly tell cmake where to install the binaries, run:

	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/path/to/install ../SLiM
	make
	make install

where `/path/to/install` is the path where you want `slim` and `eidos` installed below (they would be put in `/path/to/install/bin` in this example, analogous to the `--user` flag).

To update the build (after a `git pull`, for example), in the `SLiM_build` directory run:

	cmake -DCMAKE_BUILD_TYPE=Release ../SLiM
	make

To build a debug version of the code (i.e., optimization turned off, debugging symbols turned on, and extra runtime checking enabled), run instead:

	cmake -DCMAKE_BUILD_TYPE=Debug ../SLiM
	make

This could be done in a separate build directory to retain both versions of the build.

See the SLiM manual for more information about building and installing, including instructions on building SLiMgui (the graphical modeling environment for SLiM) on various platforms.  The manual and other SLiM stuff can be found at [http://messerlab.org/slim/](http://messerlab.org/slim/).
