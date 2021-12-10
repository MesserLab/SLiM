<p align="center">
	<img alt="Screenshot of SLiMgui running on OS X." height="45%" width="45%" src="https://messerlab.files.wordpress.com/2021/12/slimgui_screenshot.jpg"/>
</p>



<p align="center">
	SLiM: Selection on Linked Mutations
</p>
<p align="justify">
	SLiM is an evolutionary simulation framework that combines a powerful engine for population genetic simulations with the capability of modeling arbitrarily complex evolutionary scenarios. Simulations are configured via the integrated Eidos scripting language that allows interactive control over practically every aspect of the simulated evolutionary scenarios. The underlying individual-based simulation engine is highly optimized to enable modeling of entire chromosomes in large populations. We also provide a graphical user interface on macOS and Linux for easy simulation set-up, interactive runtime control, and dynamical visualization of simulation output.
</p>

GitHub Actions | Travis CI | Fedora Copr
---|---|---
![SLiM on GitHub Actions:](https://github.com/MesserLab/SLiM/workflows/tests/badge.svg) |![SLiM on Travis-CI:](https://travis-ci.com/MesserLab/SLiM.svg?branch=master) | [![Copr build status](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/)

:construction: This GitHub repository hosts the <em>upstream, development head version</em> of SLiM and SLiMgui.

:warning: <strong>End users should generally not use these sources; they may contain serious bugs, or may not even compile</strong>.

:heavy_check_mark: The <strong><em>release</em></strong> version of SLiM and SLiMgui is available at [http://messerlab.org/slim/](http://messerlab.org/slim/).


License
----------

Copyright (c) 2016-2021 Philipp Messer.  All rights reserved.

SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with SLiM.  If not, see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

Development & Feedback
-----------------------------------

SLiM is under active development, and our goal is to make it as broadly useful as possible.  If you have feedback or feature requests, or if you are interested in contributing to SLiM, please contact Philipp Messer at [messer@cornell.edu](mailto:messer@cornell.edu). Please note that Philipp is also looking for graduate students and postdocs.

Installation
------------
<em>Looking for Binary Packages / Installers?</em>
OS X | Red Hat Enterprise, CentOS, and Fedora | Debian and Ubuntu | Arch | Windows 10 (WSL & GWSL)
---|---|---|---|---
[http://messerlab.org/slim/](http://messerlab.org/slim/) | [Copr Repository](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/) | [SLiM-Extras Repository](https://github.com/MesserLab/SLiM-Extras/blob/master/installation/DebianUbuntuInstall.sh) | [Arch User Repository](https://aur.archlinux.org/packages/slim-simulator/) | [SLiM-Extras Repository](https://github.com/MesserLab/SLiM-Extras/blob/master/installation/Windows10Installation.md)


Compilation of SLiM from source
----------------------------------

See chapter two of the SLiM manual for more information about building and installing, including instructions on building SLiMgui (the graphical modeling environment for SLiM) on various platforms.  The manual and other SLiM resources can be found at [http://messerlab.org/slim/](http://messerlab.org/slim/).
