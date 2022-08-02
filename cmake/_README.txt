This folder contains a small subset of the files in:

https://github.com/rpavlik/cmake-modules

Specifically:

./LICENSE_1_0.txt
./README.markdown
./AboutTheseModules.cmake
./GetGitRevisionDescription.cmake
./GetGitRevisionDescription.cmake.in

These are distributed under the BSL 1.0 license: ./LICENSE_1_0.txt.
Note that the BSL 1.0 is compatible with SLiM's license, allowing
incorporation of these files.  See README.markdown for more info.
Thanks to Ryan A. Pavlik for these very useful modules!

----------------------------------------------------------

The purpose of including these is to allow the current Git commit
SHA1 to be shown in QtSLiM and slim's about info, making it easier
to identify the version of SLiM being used.  See:

https://stackoverflow.com/a/4318642/2752221

for details on how this is done in CMake.

Under qmake, we have a similar but inferior strategy at present
based upon https://stackoverflow.com/a/66315563/2752221 that sets
a define in QtSLiM.pro and sets a C global in GitSHA1_qmake.cpp.
It updates only when qmake is run, so the hash shown in SLiMgui
can be out of date; BEWARE.  It is intended chiefly for tagging
builds done by the various installers.

Under Xcode we have no similar scheme, and GitSHA1_Xcode.cpp sets
up a dummy string that indicates the hash is unknown.

Thanks to Bryce Carson for setting me on the trail with a link
to rpavlik's repo!

Benjamin C. Haller
2 August 2022
