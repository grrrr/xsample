/*

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2003 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
#include <stdio.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif


class xplay:
	public xinter
{
	FLEXT_HEADER_S(xplay,xinter,setup)

public:
	xplay(I argc,const t_atom *argv);
	
	virtual BL Init();
		
	virtual V m_help();
	virtual V m_print();
	
private:
	static V setup(t_classid c);

	virtual V m_signal(I n,S *const *in,S *const *out) 
	{ 
		bufchk();
		playfun(n,in,out); 
	}
};

FLEXT_LIB_DSP_V("xplay~",xplay)

V xplay::setup(t_classid c)
{
	DefineHelp(c,"xplay~");
}

xplay::xplay(I argc,const t_atom *argv)
{
	I argi = 0;
#if FLEXT_SYS == FLEXT_SYS_MAX
	if(argc > argi && CanbeInt(argv[argi])) {
		outchns = GetAInt(argv[argi]);
		argi++;
	}
#endif

	if(argc > argi && IsSymbol(argv[argi])) {
		buf = new buffer(GetSymbol(argv[argi]),true);
		argi++;
		
#if FLEXT_SYS == FLEXT_SYS_MAX
		// oldstyle command line?
		if(argi == 1 && argc == 2 && CanbeInt(argv[argi])) {
			outchns = GetAInt(argv[argi]);
			argi++;
			post("%s: old style command line detected - please change to '%s [channels] [buffer]'",thisName(),thisName()); 
		}
#endif
	}
	else
		buf = new buffer(NULL,true);

	AddInSignal("Messages and Signal of playing position");  // pos signal
	for(I ci = 0; ci < outchns; ++ci) {
		C tmp[30];
		sprintf(tmp,"Audio signal channel %i",ci+1); 
		AddOutSignal(tmp);
	}
	
	m_reset();
}

BL xplay::Init()
{
	if(xinter::Init()) {
		m_reset();
		return true;
	}
	else
		return false;
}
		


V xplay::m_help()
{
	post("%s - part of xsample objects, version " XSAMPLE_VERSION,thisName());
#ifdef FLEXT_DEBUG
	post("compiled on " __DATE__ " " __TIME__);
#endif
	post("(C) Thomas Grill, 2001-2002");
#if FLEXT_SYS == FLEXT_SYS_MAX
	post("Arguments: %s [channels=1] [buffer]",thisName());
#else
	post("Arguments: %s [buffer]",thisName());
#endif
	post("Inlets: 1:Messages/Position signal");
	post("Outlets: 1:Audio signal");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset name: set buffer");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\tprint: print current settings");
	post("\tbang/start: begin playing");
	post("\tstop: stop playing");
	post("\treset: checks buffer");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tinterp 0/1/2: set interpolation to off/4-point/linear");
	post("");
}

V xplay::m_print()
{
	const C *interp_txt[] = {"off","4-point","linear"};
	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("out channels = %i, samples/unit = %.3f, interpolation = %s",outchns,(F)(1./s2u),interp_txt[interp >= xsi_none && interp <= xsi_lin?interp:xsi_none]); 
	post("");
}


