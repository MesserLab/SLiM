<p align="center">
	<img alt="Screenshot of SLiMgui running on OS X." height="75%" width="75%" src="https://messerlab.files.wordpress.com/2021/12/slimgui_screenshot.jpg"/>
</p>



<p align="center">
	SLiM: Selection on Linked Mutations
</p>
<p align="justify">
	SLiM is an evolutionary simulation framework that combines a powerful engine for population genetic simulations with the capability of modeling arbitrarily complex evolutionary scenarios. Simulations are configured via the integrated Eidos scripting language that allows interactive control over practically every aspect of the simulated evolutionary scenarios. The underlying individual-based simulation engine is highly optimized to enable modeling of entire chromosomes in large populations. We also provide a graphical user interface on macOS and Linux for easy simulation set-up, interactive runtime control, and dynamical visualization of simulation output.
</p>

GitHub Actions | Fedora Copr | Conda
---|---|---
![SLiM on GitHub Actions:](https://github.com/MesserLab/SLiM/workflows/tests/badge.svg) | [![Copr build status](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/package/SLiM/) | [![Anaconda-Server Badge](https://anaconda.org/conda-forge/slim/badges/platforms.svg)](https://anaconda.org/conda-forge/slim)

:construction: This GitHub repository hosts the <em>upstream, development head version</em> of SLiM and SLiMgui.

:warning: <strong>End users should generally not use these sources; they may contain serious bugs, or may not even compile</strong>.

:heavy_check_mark: The <strong><em>release</em></strong> version of SLiM and SLiMgui is available at [http://messerlab.org/slim/](http://messerlab.org/slim/).


License
----------

Copyright (c) 2016-2022 Philipp Messer.  All rights reserved.

SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with SLiM.  If not, see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

Development & Feedback
-----------------------------------

SLiM is under active development, and our goal is to make it as broadly useful as possible.  If you have feedback or feature requests, or if you are interested in contributing to SLiM, please contact Philipp Messer at [messer@cornell.edu](mailto:messer@cornell.edu). Please note that Philipp is also looking for graduate students and postdocs.

Installation
------------
<em>Looking for Binary Packages / Installers?</em>

**Windows & Linux users**:
Please read the manual for information on what platforms are currently supported; this table is not always up-to-date, but has links to solutions that have previously worked for most users. Particularly, the Windows installation procedure has changed from Windows 10 and now an installer directly supporting Windows is documented in the manual. The disappearance of the Windows logo from the below table only reflects the deprecation of an outdated procedure to use SLiMgui under WSL; that procedure still works, but is outmoded/deprecated.

**Linux users**: an AppImage is now supported, rather than Flatpak (which never saw public light), and will be released shortly upon the 4.1 release of SLiM. The AppImage is a portable binary distribution that is supported on most Linux platforms, but is not foolproof as it depends on the Kernel. If the AppImage ever breaks, create an issue and tag @bryce-carson to request an update (breakage can occur when runtime libraries are outpaced by the Linux kernel, so fast-moving distributions can sometimes cause AppImages to break).

**[Download the AppImage, cross-distribution binary of SLiMgui here](https://github.com/bryce-carson/SLiM/releases)**: always download the latest release of the AppImage; sometimes there are unforseen build issues with AppImage which cause the underlying libstdc++ to become unworkable (if the AppImage was built on too new of a system). If the AppImage does not run, this is likely the case for your system and your system is too old (it is unsupported by us). The oldest distribution supported by the AppImage distributed here is Ubuntu 20.04 LTS, which is supported until 2025.

<table>
<thead>
<tr>
<th>macOS</th>
<th>Fedora, Red Hat Enterprise, CentOS, and openSUSE</th>
<th>Debian & Ubuntu</th>
<th>Arch Linux</th>
</tr>
</thead>
<tbody>
<!-- Icon-only Row -->
<tr>
<!-- macOS -->
<td align="center"><a href="http://benhaller.com/slim/SLiM_OSX_Installer.pkg" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/apple.svg" width="64" height="64"></a></td>

<!-- Fedora -->
<!-- Fedora copr Logo SVG tag upstream: https://pagure.io/copr/copr/blob/main/f/doc/img/copr-logo.svg-->
<td align="center"><a href="https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/fedora.svg" alt="Fedora" width="64" height="64"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/opensuse.svg" alt="openSUSE" width="64" height="64"></a></td>

<!-- Debian & Ubuntu Shell Script -->
<td align="center"><a href="https://github.com/MesserLab/SLiM-Extras/blob/master/installation/DebianUbuntuInstall.sh" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/debian.svg" alt="Debian" width="64" height="64"></a></td>

<!-- Arch Linux -->
<td align="center"><a href="https://aur.archlinux.org/packages/slim-simulator/" target="_blank" rel="noopener noreferrer"><img src="https://raw.githubusercontent.com/JunaidQadirB/font-os/master/vectors/archlinux.svg" alt="Arch" width="64" height="64"></a></td>

</tr>

<!-- Text-only Row -->
<tr>

<!-- macOS -->
<td align="center"><a href="http://benhaller.com/slim/SLiM_OSX_Installer.pkg" target="_blank" rel="noopener noreferrer">Messer Lab Website</a></td>

<!-- Fedora -->
<td align="center"><a href="https://copr.fedorainfracloud.org/coprs/bacarson/SLiM-Selection_on_Linked_Mutations/" target="_blank" rel="noopener noreferrer">Copper Repository</a></td>

<!-- Debian & Ubuntu Shell Script -->
<td align="center"><a href="https://github.com/MesserLab/SLiM-Extras/blob/master/installation/DebianUbuntuInstall.sh" target="_blank" rel="noopener noreferrer">SLiM-Extras MD Document</a></td>

<!-- Arch Linux -->
<td align="center"><a href="https://aur.archlinux.org/packages/slim-simulator/" target="_blank" rel="noopener noreferrer">Arch User Repository</a></td>

</tr>
</tbody>
</table>

Compilation of SLiM from source
----------------------------------

See chapter two of the SLiM manual for more information about building and installing, including instructions on building SLiMgui (the graphical modeling environment for SLiM) on various platforms.  The manual and other SLiM resources can be found at [http://messerlab.org/slim/](http://messerlab.org/slim/).
