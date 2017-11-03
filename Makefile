# Makefile for the "slim" and "eidos" command-line tools
#
# Created by Ben Haller on 26 February 2016.
# Copyright (c) 2016 Philipp Messer.  All rights reserved.
# A product of the Messer Lab, http://messerlab.org/software/
#
#
# This file is part of SLiM.
#
# SLiM is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License,
# or (at your option) any later version.
#
# SLiM is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.

#
# Note that if you are on OS X, you should build all SLiM targets in Xcode, not using make!
# Note also that this makefile is very dumb.  If somebody wants to make it smarter, feel free.
# However, development of SLiM is done exclusively on OS X, so this Makefile is really just
# used to build SLiM on a user's machine; doing minimal builds is really not important.
#

#
# At present this Makefile uses g++ to compile everything, which means that the GSL sources
# are compiled as C++ even though they are .c files.  This may lead to a warning (e.g., in
# clang) like: "treating 'c' input as 'c++' when in C++ mode, this behavior is deprecated".
# I think this is harmless, but it would probably be better to use gcc for the .c files,
# generate objects, and link it all together at the end.  I.e., to have a real Makefile.
# I am far from a Makefile jockey, so I am just going to hope that somebody else comes
# along and does this for us.   :->
#

SHELL = /bin/sh
CC = g++
CFLAGS = -O3 -Wno-deprecated-register
INCLUDES = -iquote./eidos -iquote./gsl -iquote./gsl/rng -iquote./gsl/randist -iquote./gsl/sys -iquote./gsl/specfunc -iquote./gsl/cdf -iquote./gsl/complex
ALL_CFLAGS = $(CFLAGS) $(INCLUDES) -std=c++11

all: slim eidos FORCE

slim: FORCE
	mkdir -p ./bin
	$(CC) $(ALL_CFLAGS) ./core/*.cpp ./eidos/*.cpp ./gsl/*/*.c -o ./bin/slim
	@echo
	@echo "Build completed."
	@echo
	@echo "If you use SLiM 2 in a publication, please cite:"
	@echo
	@echo "    BC Haller, PW Messer. (2017).  SLiM 2: Flexible, interactive forward"
	@echo "        genetic simulations.  Molecular Biology and Evolution 34 (1), 230-240."
	@echo

eidos: FORCE
	mkdir -p ./bin
	$(CC) $(ALL_CFLAGS) ./eidostool/*.cpp ./eidos/*.cpp ./gsl/*/*.c -o ./bin/eidos

clean: FORCE
	rm -f ./bin/slim
	rm -f ./bin/eidos

# Would use .PHONY, but we don't want to depend on GNU make.
# See http://www.gnu.org/software/make/manual/make.html#Phony-Targets
# and http://www.gnu.org/software/make/manual/make.html#Force-Targets
FORCE:
