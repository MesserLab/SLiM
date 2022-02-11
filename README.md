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
<table>
<thead>
<tr>
<th>macOS</th>
<th>Windows 10</th>
<th>Fedora, Red Hat Enterprise, CentOS, and openSUSE</th>
<th>Debian & Ubuntu</th>
<th>Ubuntu 20.04 LTS (Focal Fossa)</th>
<th>Arch Linux</th>
</tr>
</thead>
<tbody>
<!-- Icon-only Row -->
<tr>
<!-- macOS -->
<td align="center"><a href="http://benhaller.com/slim/SLiM_OSX_Installer.pkg" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/apple.svg" width="64" height="64"></a></td>

<!-- Windows 10 -->
<td align="center"><a href="https://github.com/MesserLab/SLiM-Extras/blob/master/installation/Windows10Installation.md" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/win10.svg" alt="Windows 10" width="64" height="64"></a></td>

<!-- Fedora -->
<!-- Fedora copr Logo SVG tag upstream: https://pagure.io/copr/copr/blob/main/f/doc/img/copr-logo.svg-->
<td align="center"><a href="https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/fedora.svg" alt="Fedora" width="64" height="64"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/opensuse.svg" alt="openSUSE" width="64" height="64"></a></td>

<!-- Debian & Ubuntu Shell Script -->
<td align="center"><a href="https://github.com/MesserLab/SLiM-Extras/blob/master/installation/DebianUbuntuInstall.sh" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/debian.svg" alt="Debian" width="64" height="64"></a></td>

<!-- Ubuntu PPA -->
<td align="center"><a href="https://launchpad.net/~bryce-a-carson/+archive/ubuntu/slim-sim" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/ubuntu.svg" alt="Ubuntu" width="64" height="64"></a></td>

<!-- Arch Linux -->
<td align="center"><a href="https://aur.archlinux.org/packages/slim-simulator/" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/archlinux.svg" alt="Arch" width="64" height="64"></a></td>

</tr>

<!-- Text-only Row -->
<tr>

<!-- macOS -->
<td align="center"><a href="http://benhaller.com/slim/SLiM_OSX_Installer.pkg" target="_blank" rel="noopener noreferrer">Messer Lab Website</a></td>

<!-- Windows 10 -->
<td align="center"><a href="https://github.com/MesserLab/SLiM-Extras/blob/master/installation/Windows10Installation.md" target="_blank" rel="noopener noreferrer">SLiM-Extras MD Document</a></td>

<!-- Fedora -->
<td align="center"><a href="https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/" target="_blank" rel="noopener noreferrer">Copper Repository</a></td>

<!-- Debian & Ubuntu Shell Script -->
<td align="center"><a href="https://github.com/MesserLab/SLiM-Extras/blob/master/installation/DebianUbuntuInstall.sh" target="_blank" rel="noopener noreferrer">SLiM-Extras MD Document</a></td>

<!-- Ubuntu PPA -->
<td align="center"><a href="https://launchpad.net/~bryce-a-carson/+archive/ubuntu/slim-sim" target="_blank" rel="noopener noreferrer">Launchpad (PPA)</a></td>

<!-- Arch Linux -->
<td align="center"><a href="https://aur.archlinux.org/packages/slim-simulator/" target="_blank" rel="noopener noreferrer">Arch User Repository</a></td>

</tr>
</tbody>
</table>

Compilation of SLiM from source
----------------------------------

See chapter two of the SLiM manual for more information about building and installing, including instructions on building SLiMgui (the graphical modeling environment for SLiM) on various platforms.  The manual and other SLiM resources can be found at [http://messerlab.org/slim/](http://messerlab.org/slim/).
