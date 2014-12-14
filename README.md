SLiM
====

**S**election on **Li**nked **M**utations: a forward population genetic simulation for studying linkage effects, such as hitchhiking, background selection, and Hill-Robertson interference.

SLiM is a product of the Messer Lab at Cornell University. It was developed by Philipp Messer, and is now maintained and extended by Philipp Messer and Ben Haller.

GitHub home page for SLiM: [https://github.com/MesserLab/SLiM](https://github.com/MesserLab/SLiM)

Messer Lab home page for SLiM: [http://messerlab.org/software/](http://messerlab.org/software/)

License
----------

Copyright (c) 2014 Philipp Messer.  All rights reserved.

This file is part of SLiM.

SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with SLiM.  If not, see [http://www.gnu.org/licenses/](http://www.gnu.org/licenses/).

Usage
---------
For Mac OS X users, an Xcode project is provided that can be used to build SLiM. For users on other platforms, or for those who prefer not to use Xcode, SLiM can be built in Terminal with the command:

```
cd SLiM
<commands to be supplied here>
```

Once SLiM is built, just run it at Terminal's command line, like:

```
slim <input-file>
```

Examples
-------------
Some example input files that illustrate how to use SLiM are provided as part of this distribution, in the *examples* folder.  If you don't have that folder, please visit the SLiM's GitHub page (link above).

For example, assuming you have built SLiM inside its own folder, you can run the first example file in Terminal by executing:

```
cd SLiM
slim ./input_example_1.txt
```

Development & Feedback
-----------------------------------
SLiM is under active development, and our goal is to make it as broadly useful as possible.  If you have feedback or feature requests, or if you are interested in contributing to SLiM, please contact Philipp Messer at [messer@cornell.edu](mailto:messer@cornell.edu). Please note that Philipp is also looking for graduate students and postdocs.

References
---------------
*The original paper on SLiM:*

Messer, P.W. (2013). SLiM: Simulating evolution with selection and linkage. *Genetics 194*(4), 1037-1039.

*Papers that have used SLiM:*

Enard, D., Messer, P. W., & Petrov, D. A. (2014). Genome-wide signals of positive selection in human evolution. *Genome research*.

**others?**

**discussion of authorship/citation in papers that use SLiM?**