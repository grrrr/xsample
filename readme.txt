xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

----------------------------------------------------------------------------

You will need the flext C++ compatibility layer for PD and Max/MSP externals to compile this.


Package files:
- readme.txt: this one
- gpl.txt,license.txt: GPL license stuff
- main.h,main.cpp: base class definition for all the other objects
- record.cpp: xrecord~
- play.cpp: xplay~
- groove.cpp: xgroove~

----------------------------------------------------------------------------

The package should at least compile (and is tested) with the following compilers:

- pd - Windows:
o Borland C++ 5.5 (free): just ok! - makefile is no real make but works
o Microsoft Visual C++ 6: due to a compiler bug the optimization using templates is not functional

- pd - linux:
o GCC for linux: the compiler (version 2.95.2) dies when template optimization is turned on 

- Max/MSP - MacOS:
o Metrowerks CodeWarrior V6: can't get address of a template function

----------------------------------------------------------------------------

TODO list:
- Documentation and better example patches
- cleaner makefile PD/Borland C++
- makefile and build for PD/GCC
- eventually make use of resource files for text items
- as template <int> functions don't work for most compilers: find another way for optimizations
- crossfading loop zone for xgroove~

