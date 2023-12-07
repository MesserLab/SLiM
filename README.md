<p align="center">
	<img alt="Screenshot of SLiMgui running on OS X." height="75%" width="75%" src="https://messerlab.files.wordpress.com/2021/12/slimgui_screenshot.jpg"/>
</p>



<p align="justify">
	SLiM is an evolutionary simulation framework that combines a powerful engine for population genetic simulations with the capability of modeling arbitrarily complex evolutionary scenarios. Simulations are configured via the integrated Eidos scripting language that allows interactive control over practically every aspect of the simulated scenarios. The underlying individual-based simulation engine is highly optimized to enable modeling of entire chromosomes in large populations. We also provide a graphical user interface called SLiMgui on macOS, Linux, and Windows for easy simulation set-up, interactive runtime control, and dynamic visualization of simulation output.
</p>

GitHub Actions | Fedora Copr | Conda
---|---|---
![SLiM on GitHub Actions:](https://github.com/MesserLab/SLiM/workflows/tests/badge.svg) | [![Copr build status](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/) | [![Anaconda-Server Badge](https://anaconda.org/conda-forge/slim/badges/platforms.svg)](https://anaconda.org/conda-forge/slim)

:construction: This GitHub repository hosts the <em>upstream, development head version</em> of SLiM and SLiMgui.

:warning: <strong>End users should generally not use these sources; they may contain serious bugs, or may not even compile</strong>.

:heavy_check_mark: The <strong><em>release</em></strong> version of SLiM and SLiMgui is available at [http://messerlab.org/slim/](http://messerlab.org/slim/).


License
----------

Copyright (c) 2016-2023 Philipp Messer.  All rights reserved.

SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with SLiM.  If not, see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

Development & Feedback
-----------------------------------

SLiM is under active development, and our goal is to make it as broadly useful as possible.  If you have feedback or feature requests, or if you are interested in contributing to SLiM, please contact Philipp Messer at [messer@cornell.edu](mailto:messer@cornell.edu). Please note that Philipp is also looking for graduate students and postdocs.

Installation
------------
<em>Looking for Binary Packages / Installers?</em>

The following subsections summarize what methods for acquiring SLiM (and SLiMgui) are available.  Building from sources is also an option on all platforms; see the next section.

##### macOS
https://github.com/MesserLab/SLiM/releases/download/v4.0.1/SLiM_OSX_Installer.pkg

##### Linux
###### Arch & Manjaro
Any Arch-based distributions *which support the AUR* should be compatibile.

https://aur.archlinux.org/packages/slim-simulator/

###### Fedora, Red Hat, openSUSE
Derivative distributions are not guaranteed compatibility with these binary packages. Enable the repository for your operating system; you might also try using the source RPM package to rebuild the package for your system to give you an excellent integration for any RPM-based distribution.

https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/

###### Debian & Ubuntu (and any derivatives using dpkg)
A shell script using the facilities of `dpkg` is available. It uses the CMake install target to integrate SLiMgui with the desktop environment. It has the advantage over building from source in that it will check build dependencies for you, and it will automatically remove build artifacts from `/tmp`. Source the script with `curl` following the instructions in the manual.

https://raw.githubusercontent.com/MesserLab/SLiM-Extras/master/installation/DebianUbuntuInstall.sh

##### Windows (10 & 11)
###### Native package (using MSYS2)
The installation instructions for the native package (using MSYS2) are available in the Manual.

###### WSL2 installation guide
This guide shows how to use an XServer to display graphical applications from WSL2 on your machine. The instructions are out of date, but may still prove useful.

Most of the instructions in the guide should be avoided, due to the subsystem for Linux now supporting graphical applications out-of-the-box: https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps.

The last instruction, on sourcing the Debian & Ubuntu shell script, are still relevant if your preference is to use WSL2 rather than MSYS2.

https://github.com/MesserLab/SLiM-Extras/blob/master/installation/Windows10Installation.md


Compilation of SLiM from Source
----------------------------------

See chapter two of the SLiM manual for more information about building and installing, including instructions on building SLiMgui (the graphical modeling environment for SLiM) on various platforms.  The manual and other SLiM resources can be found at [http://messerlab.org/slim/](http://messerlab.org/slim/).
