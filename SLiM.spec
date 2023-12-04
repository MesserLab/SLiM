# Cross-distribution SLiM RPM spec.
# Defines:
%if %{defined suse_version}
%define qt5 libqt5
%else
%define qt5 qt5
%endif

Name:           SLiM
Version:        4.1
Release:        1%{?dist}
Summary:        an evolutionary simulation framework

License:        GPLv3+
URL:            https://messerlab.org/slim/
Source0:        https://github.com/MesserLab/SLiM/archive/v%{version}.tar.gz

# Prevent users of the Copr repository from using Simple Login Manager, due to binary file name conflict.
Conflicts:      slim

# This paragraph of the spec file is old and delicate.
BuildRequires:  cmake
# openSUSE Build Requires
%if %{defined suse_version}
%if 0%{?suse_version} >= 1500
BuildRequires:  glew-devel
BuildRequires:  Mesa-libGL-devel
BuildRequires:  gcc-c++
BuildRequires:  libqt5-qtbase-devel
BuildRequires:  appstream-glib-devel
%endif
%else
BuildRequires:  qt5-qtbase-devel
BuildRequires:  libappstream-glib
%endif
ExclusiveArch:  x86_64

# RHEL 8 has the oldest point release of 5.15, and is the oldest RHEL supported.
Requires: %{qt5}-qtbase >= 5.15.1

%description
SLiM is an evolutionary simulation framework that combines a powerful engine for
population genetic simulations with the capability of modeling arbitrarily
complex evolutionary scenarios. Simulations are configured via the integrated
Eidos scripting language that allows interactive control over practically every
aspect of the simulated evolutionary scenarios. The underlying individual-based
simulation engine is highly optimized to enable modeling of entire chromosomes
in large populations. We also provide a graphical user interface on macOS and
Linux for easy simulation set-up, interactive runtime control, and dynamical
visualization of simulation output.

%prep
%setup -q

%build
%cmake -DBUILD_SLIMGUI=ON
%cmake_build

%install
%cmake_install

%files
%{_bindir}/eidos
%{_bindir}/slim
%{_bindir}/SLiMgui
%{_datadir}/applications/org.messerlab.slimgui.desktop
%{_datadir}/icons/hicolor/scalable/apps/org.messerlab.slimgui.svg
%{_datadir}/icons/hicolor/scalable/mimetypes/text-slim.svg
%{_datadir}/icons/hicolor/symbolic/apps/org.messerlab.slimgui-symbolic.svg
%{_datadir}/metainfo/org.messerlab.slimgui.appdata.xml
%{_datadir}/metainfo/org.messerlab.slimgui.metainfo.xml
%{_datadir}/mime/packages/org.messerlab.slimgui-mime.xml

%changelog
* Mon Dec 4 2023 Bryce Carson <bryce.a.carson@gmail.com> - 4.1-1
- Final candidate 1 for 4.1 release
- CMake install of package desktop environment data properly implemented
- RPM macros adopted

* Tue Sep 27 2022 Bryce Carson <bryce.a.carson@gmail.com> - 4.0.1-2
- `CMakeLists.txt` improved, so the installation section of the RPM is now simplified.
- Data files now exist in `data/`, rather than in the root folder of the software.

* Tue Sep 13 2022 Ben Haller <bhaller@mac.com> - 4.0.1-1
- Final candidate 1 for 4.0.1 release

* Tue Aug 23 2022 Bryce Carson <bryce.a.carson@gmail.com> - 4.0-2
- Include new changelog entry to identify the date of the new release

* Wed Aug 10 2022 Bryce Carson <bryce.a.carson@gmail.com> - 4.0-1
- New release

* Sat Feb 12 2022 Bryce Carson <bryce.a.carson@gmail.com> - 3.7.1-1
- Increment version

* Wed Dec 15 2021 Ben Haller <bhaller@mac.com> - 3.7-1
- Final candidate 1 for 3.7 release
- Removed robinhood patch
- Removed --parallel for cmake since it was no longer working

* Sat Apr 24 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.6-5
- Fixed email address in previous changelog entry.
- Included a conflict tag to prevent users of this package from using the conflicting binary in Simple Login Manager.

* Sat Mar 20 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.6-4
- Added support for openSUSE (with SUSE Linux Enterprise users possibly able to use the openSUSE RPM).
- Cleaned up the changelog.
- The `[<jobs>]` argument to the cmake `--parallel` option was removed, so that Copr uses the default number of concurrent processes (and hopefully the maximum number, rather than hardcoding eight processes).

* Wed Mar 3 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.6-3
- Application of patch to allow building on Fedora 34 and Fedora Rawhide.

* Wed Mar 3 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.6-2
- Specified required Qt 5.15.2 on Fedora 34.
- Added package version in previous changelog entry.

* Wed Mar 3 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.6-1
- New package release.
- Removed source edits that were addressed upstream.

* Sun Jan 31 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.5-6
- spec file improvements; brace expansion used and sorting performed.
- FreeDesktop compliance improvements; the organization domain and application name are corrected and are now compliant.
- Source modifications allow Gnome Classic to display the proper application name.
- The symbolic application icon is now created programmatically from upstream icons in the source, rather than a second source file.

* Thu Jan 28 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.5-5
- org.messerlab.slimgui.desktop changed in prep to correct Categories value; fixes desktop integration on Fedora 33 when using Gnome Classic environment.
- New symbolic icon included; improves desktop integration on Fedora 33 when using Gnome 3 with Wayland.
- Edited the changelog to not refer to the prep stage as a macro, simply "prep", to fix rpmlint warnings.

* Thu Jan 14 2021 Bryce Carson <bryce.a.carson@gmail.com> - 3.5-4
- org.messerlab.slimgui.desktop changed in prep to correct StartupWMClass and icon; fixes desktop integration on Fedora 33.
- Sorted changelog in descending chronological order.

* Sun Dec 06 2020 Bryce Carson <bryce.a.carson@gmail.com> - 3.5-3
- Updated the requires in the .spec file (and thus the package dependencies) to reflect updates to Qt5 on Fedora 33.
- Qt5 5.15.2 now required on Fedora 33.

* Sun Dec 06 2020 Bryce Carson <bryce.a.carson@gmail.com> - 3.5-2
- Changed the tar command in .spec file to address discrepancy between GitHub archive URI and downloaded source archive.

* Sun Dec 06 2020 Bryce Carson <bryce.a.carson@gmail.com> - 3.5-1
- Created new release package
- Differences from 3.4-8 include removal of necessary source modifications for 3.4
- The source modifications for 3.4 were addressed by the upstream
