xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

Donations for further development of the package are highly appreciated.

----------------------------------------------------------------------------

IMPORTANT INFORMATION for all MaxMSP users:

It is advisable to put the xsample shared library file into the "startup" folder. Hence it will be
loaded at Max startup.

If you want to load the xsample library on demand, please run MakeAliases inside the MPW folder.
This will create aliases to the several xsample objects contained in the library. 
Move these into the "externals" folder.
(This latter procedure is only tested for OS 9.2, you may experience problems with other MacOS versions)

----------------------------------------------------------------------------

You will need the flext C++ layer for PD and Max/MSP externals to compile this.


Package files:
- readme.txt: this one
- gpl.txt,license.txt: GPL license stuff
- main.h,main.cpp,inter.cpp,inter.ci: base class definition for all the other objects
- record.cpp: xrecord~
- play.cpp: xplay~
- groove.cpp: xgroove~

----------------------------------------------------------------------------

The package should at least compile (and is tested) with the following compilers:

pd - Windows:
-------------
o Borland C++ 5.5 (free): edit & run "make -f makefile.bcc"

o Microsoft Visual C++ 6: edit "xsample.dsp" project file
> due to a compiler bug the optimization using templates is not functional

pd - linux:
-----------
o GCC for linux: run "make -f makefile.pd-linux" and as root "make -f makefile.pd-linux install"
> various versions of GCC die during compile with template optimization turned on 

Max/MSP - MacOS:
----------------
o Metrowerks CodeWarrior V6: edit "xsample.cw" project file functions

o Apple MPW-PR: edit & use the "flext.mpw" makefile


----------------------------------------------------------------------------

Goals/features of the package:

- portable and effective sample recording/playing objects for pd and Max/MSP
- MSP-like groove~ object for PD 
- message- or signal-triggered recording object with mix-in capability
- avoid the various bugs of the original MSP2 objects
- multi-channel capability 
- live update of respective buffer/array content
- switchable 4-point or linear interpolation for xplay~/xgroove~ object
- cross-fading loop zone (inside or outside to loop) for xgroove~

----------------------------------------------------------------------------

Version history:

0.2.5:
- added resources to MaxMSP build
- xgroove~, xrecord~: introduced a loop/end bang outlet 
- added MaxMSP buffer resize recognition
- xgroove~: introduced a crossfading loop zone
- adapted source for flext 0.4.1
- introduced attributes

0.2.4:
- according to flext 0.2.3 changed sample type to t_sample (S)
- xrecord~: fixed mix mode bug
- fixed argument buffer problem

0.2.3:
- using flext 0.2.2 - xsample is now a library under MaxMSP
- cleaner gcc makefile
- xgroove~, xrecord~: added "all" message to select entire buffer length
- xgroove~, xplay~: revisited dsp methods, restructured the code, fixed small interpolation bugs 
- xgroove~, xplay~: added linear interpolation (message "interp 2") 
- enabled 0 output channels -> xgroove~: position output only
- xgroove~: added bidirectional looping (message "loop 2")

0.2.2:
- using flext 0.2.0
- xrecord~ for PD: new flext brings better graphics update behavior
- xrecord~: recording position doesn't jump to start when recording length is reached
- fixed bug with refresh message (min/max reset)
- xgroove~: position (by pos message) isn't sample rounded anymore
- reset/refresh messages readjust dsp routines to current buffer format (e.g. channel count)
- corrected Max/MSP assist method for multi-channel
- fixed xplay~ help method 
- changed syntax to x*~ [channels=1] [buffer] for future enhancements (MaxMSP only, warning for old syntax)
- fixed small bug concerning startup position in xgroove~ and xrecord~
- fixed deadly bug in xplay~ dsp code (only active with template optimization) 

0.2.1:
- no leftmost float inlet for position setting - use pos method
- changed dsp handling for flext 0.1.1 conformance
- workarounds for buggy/incomplete compilers
- prevent buffer warning message at patcher load (wait for loadbang)
- fixed bug: current pos is reset when changing min or max points

0.2.0: 
- first version for flext

---------------------------------------------------------------------------


TODO list:

general:
- Documentation and better example patches

- do a smooth (line~) mixin in xrecord~ help patch

features:
- multi-buffer handling (aka multi-channel for pd)
- vasp handling
- performance comparison to respective PD/Max objects
- anti-alias filter? (possible?)

- delay min/max changes when cur pos in cross-fade zone

tests:
- reconsider startup sequence of set buffer,set units,set sclmode,set pos/min/max

bugs:
- PD: problems with timed buffer redrawing (takes a lot of cpu time) - flext bug?
- Apple MPW doesn't correctly compile template optimization 
- Max help files aren't correctly opened due to xsample objects residing in a library
