<p align="center">
	<img alt="Screenshot of SLiMgui running on OS X." height="45%" width="45%" src="https://messerlab.files.wordpress.com/2016/02/slimgui_screenshot.jpg?w=600"/>
</p>



<p align="center">
	:dna: SLiM: Selection on Linked Mutations :dna:
</p>
<p align="center">
	SLiM is an evolutionary simulation framework that combines a powerful engine for population genetic simulations with the capability of modeling arbitrarily complex evolutionary scenarios. Simulations are configured via the integrated Eidos scripting language that allows interactive control over practically every aspect of the simulated evolutionary scenarios. The underlying individual-based simulation engine is highly optimized to enable modeling of entire chromosomes in large populations. We also provide a graphical user interface on macOS and Linux for easy simulation set-up, interactive runtime control, and dynamical visualization of simulation output.
</p>

GitHub Actions | Travis CI | Fedora Copr
---|---|---
![SLiM on GitHub Actions:](https://github.com/MesserLab/SLiM/workflows/tests/badge.svg) |![SLiM on Travis-CI:](https://travis-ci.com/MesserLab/SLiM.svg?branch=master) | [![Copr build status](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/)

:construction: This GitHub repository hosts the <em>upstream, development head version</em> of SLiM and SLiMgui.

:warning: End-users should <strong>not</strong> use these sources.

:heavy_check_mark: The <strong><em>release</em></strong> version of SLiM and SLiMgui are available at [http://messerlab.org/slim/](http://messerlab.org/slim/).


:copyright: License
----------

Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.

SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with SLiM.  If not, see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

:speaking_head: Development & Feedback
-----------------------------------

SLiM is under active development, and our goal is to make it as broadly useful as possible.  If you have feedback or feature requests, or if you are interested in contributing to SLiM, please contact Philipp Messer at [messer@cornell.edu](mailto:messer@cornell.edu). Please note that Philipp is also looking for graduate students and postdocs.

:floppy_disk: Compilation of SLiM from source
----------------------------------

See the SLiM manual for more information about building and installing, including instructions on building SLiMgui (the graphical modeling environment for SLiM) on various platforms.  The manual and other SLiM stuff can be found at [http://messerlab.org/slim/](http://messerlab.org/slim/).

:eyes: <em>Looking for Binary Packages (Installers)?</em>
OS X | Red Hat Enterprise, CentOS, and Fedora | Debian and Ubuntu | Arch
---|---|---|---
[http://messerlab.org/slim/](http://messerlab.org/slim/) | [Copr Repository](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/) | [SLiM-Extras Repository](https://github.com/MesserLab/SLiM-Extras/blob/master/installation/DebianUbuntuInstall.sh) | [Arch User Repository](https://aur.archlinux.org/packages/slim-simulator/)

If you are interested in <strong>testing</strong> the development version of SLiM, you can expand the information below to see how to compile the development head version of SLiM.

<details>
	<summary><em>Compiling the Development Version from GitHub Sources</em></summary><p></p>
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
</details>
