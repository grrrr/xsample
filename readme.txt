xsample - extended sample objects for Max/MSP and pd (pure data)

This package is covered by the GPL. See license terms in license.txt.

You will need the max-pd C++ compatibility layer for PD and Max/MSP externals to compile this.

The package should at least compile (and is tested) with the following compilers:
- for pd:
o Microsoft Visual C++ 6: due to a compiler bug the optimization using templates is not functional
o Borland C++ 5.5 (free): this compiler handles templates correctly - the package compiles but a proper makefile doesn't exist yet.
o GCC for linux: the compiler (version 2.95.2) dies when template optimization is turned on - hence it is commented out
- for Max/MSP:
o Metrowerks CodeWarrior V6: works just perfectly


Files:
- main.h,main.cpp: base class definition for all the other objects
- record.cpp: xrecord~
- play.cpp: xplay~
- groove.cpp: xgroove~


TODO list:
- Documentation and better example patches
- makefile and build for PD/Borland C++
- makefile and build for PD/GCC
- eventually make use of resource files for text items

