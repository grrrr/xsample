/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
#include <math.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif


class xspeed:
	public xsample
{
//	FLEXT_HEADER_S(xspeed,xsample,setup)
	FLEXT_HEADER(xspeed,xsample)

public:
	xspeed(I argc,t_atom *argv);

#ifdef MAXMSP
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();

	virtual I m_set(I argc,t_atom *argv);

	virtual V m_start();
	virtual V m_stop();

	virtual V m_units(xs_unit mode = xsu__);
	virtual V m_sclmode(xs_sclmd u = xss__);

	virtual V m_pos(F pos);
	virtual V m_all();
	virtual V m_min(F mn);
	virtual V m_max(F mx);
	
	virtual V m_loop(BL lp);
	
	virtual V m_reset();
	virtual V m_refresh();

protected:

	BL doplay,doloop;
	D curpos;  // in samples

	outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { ToOutFloat(outmin,curmin*s2u); }
	V outputmax() { ToOutFloat(outmax,curmax*s2u); }
	
//	I bcnt,bframes;
//	F **bvecs;

	V DownLink(const C *s,I argc,t_atom *argv) { ToOutAnything(1,MakeSymbol(s),argc,argv); }
	V DownLink(const C *s) { DownLink(s,0,NULL); }
	V DownLink(const C *s,I v) { t_atom msg; SetFlint(msg,v); DownLink(s,1,&msg); }
	V DownLink(const C *s,F v) { t_atom msg; SetFloat(msg,v); DownLink(s,1,&msg); }

private:
	static V setup(t_class *c);

	virtual V s_dsp();

	DEFSIGFUN(xspeed)	
	TMPLDEF V signal(I n,F *const *in,F *const *out);  // this is my dsp method

	FLEXT_CALLBACK_F(m_pos)
	FLEXT_CALLBACK(m_all)
	FLEXT_CALLBACK_F(m_min)
	FLEXT_CALLBACK_F(m_max)

	FLEXT_CALLBACK_B(m_loop)
};


FLEXT_LIB_TILDE_G("xspeed~",xspeed)

/*
V xspeed::setup(t_class *)
{
#ifndef PD
	post("loaded xspeed~ - part of xsample objects, version " XSAMPLE_VERSION " - (C) Thomas Grill, 2001-2002");
#endif
}
*/

xspeed::xspeed(I argc,t_atom *argv):
	doplay(false),doloop(true),
	curpos(0)
{
	I outchns = 0;
	I argi = 0;
#ifdef MAXMSP
	if(argc > argi && IsFlint(argv[argi])) {
		outchns = GetAFlint(argv[argi]);
		argi++;
	}
#endif

	if(argc > argi && IsSymbol(argv[argi])) {
		buf = new buffer(GetSymbol(argv[argi]),true);
		argi++;
		
#ifdef MAXMSP
		// oldstyle command line?
		if(argi == 1 && argc == 2 && IsFlint(argv[argi])) {
			outchns = GetAFlint(argv[argi]);
			argi++;
			post("%s: old style command line detected - please change to '%s [channels] [buffer]'",thisName(),thisName()); 
		}
#endif
	}
	else
		buf = new buffer(NULL,true);		

	AddInSignal(); // speed signal
	AddInFloat(2); // min & max play pos
	AddOutSignal(); // position
	AddOutAnything(); // downlink to xplay~
	AddOutFloat(2); // play min & max	
	SetupInOut();

	FLEXT_ADDMETHOD(1,m_min);
	FLEXT_ADDMETHOD(2,m_max);
	FLEXT_ADDMETHOD_F(0,"min",m_min); 
	FLEXT_ADDMETHOD_F(0,"max",m_max);
	FLEXT_ADDMETHOD_F(0,"pos",m_pos);
	FLEXT_ADDMETHOD_(0,"all",m_all);

	FLEXT_ADDMETHOD_B(0,"loop",m_loop);

	outmin = GetOut(2);
	outmax = GetOut(3);
}


V xspeed::m_units(xs_unit mode)
{
	xsample::m_units(mode);
	
	m_sclmode();
	outputmin();
	outputmax();

	DownLink("units",(I)mode);
}

V xspeed::m_sclmode(xs_sclmd mode)
{
	xsample::m_sclmode(mode);

	DownLink("sclmode",(I)mode);
}

V xspeed::m_min(F mn)
{
	xsample::m_min(mn);
	m_pos(curpos*s2u);
	outputmin();

//	DownLink("min",mn);
}

V xspeed::m_max(F mx)
{
	xsample::m_max(mx);
	m_pos(curpos*s2u);
	outputmax();

//	DownLink("max",mx);
}

V xspeed::m_pos(F pos)
{
	curpos = pos?pos/s2u:0;
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}

V xspeed::m_all()
{
	xsample::m_all();
	outputmin();
	outputmax();

//	DownLink("all");
}

I xspeed::m_set(I argc,t_atom *argv)
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units(); 

	DownLink("set",argc,argv);
	return r;
}

V xspeed::m_start() 
{ 
	m_refresh(); 
	doplay = true; 
	s_dsp();

	DownLink("start");
}

V xspeed::m_stop() 
{ 
	doplay = false; 
	s_dsp();

	DownLink("stop");
}

V xspeed::m_reset()
{
	curpos = 0;
	xsample::m_reset();

	DownLink("reset");
}


V xspeed::m_refresh()
{
	xsample::m_refresh();
	DownLink("refresh");
}


V xspeed::m_loop(BL lp) 
{ 
	doloop = lp; 
	s_dsp();
}
	

TMPLDEF V xspeed::signal(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,0,OCHNS,0);

	const F *speed = invecs[0];
	F *const *sig = outvecs;
	F *pos = outvecs[0];

	const I smin = curmin,smax = curmax;
	const I plen = curlen;
	D o = curpos;

	if(doplay && plen > 0) {
//		register I si = 0;

		for(I i = 0; i < n; ++i) {	
			const F spd = *(speed++);  // must be first because the vector is reused for output!

			// normalize offset
			if(o < smin) 
				o = doloop?(fmod(o+smax,plen)+smin):smin;
			else if(o >= smax) 
				o = doloop?(fmod(o,plen)+smin):smax;

			*(pos++) = scale(o);
			o += spd;
		}
	} 
	else {
		register const F oscl = scale(o);
		for(I si = 0; si < n; ++si) *(pos++) = oscl;
	}
	
	// normalize and store current playing position
	if(o < smin) curpos = smin;
	else if(o >= smax) curpos = smax;
	else curpos = o;
}

V xspeed::s_dsp()
{
	sigfun = SIGFUN(xspeed,signal,-1,-1);
}



V xspeed::m_help()
{
	post("%s - part of xsample objects, version " XSAMPLE_VERSION,thisName());
#ifdef _DEBUG
	post("compiled on " __DATE__ " " __TIME__);
#endif
	post("(C) Thomas Grill, 2001-2002");
#ifdef MAXMSP
	post("Arguments: %s [channels=1] [buffer]",thisName());
#else
	post("Arguments: %s [buffer]",thisName());
#endif
	post("Inlets: 1:Messages/Speed signal, 2:Min position, 3:Max position");
	post("Outlets: 1:Audio signal, 2:Position signal, 3:Min position (rounded), 4:Max position (rounded)");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset [name]: set buffer or reinit");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\treset: reset min/max playing points and playing offset");
	post("\tprint: print current settings");
	post("\tloop 0/1: switches looping off/on");
	post("\tmin {unit}: set minimum playing point");
	post("\tmax {unit}: set maximum playing point");
	post("\tall: select entire buffer length");
	post("\tpos {unit}: set playing position (obeying the current scale mode)");
	post("\tbang/start: start playing");
	post("\tstop: stop playing");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tsclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("");
} 

V xspeed::m_print()
{
	static const C *sclmode_txt[] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("samples/unit = %.3f, scale mode = %s, loop = %s",(F)(1./s2u),sclmode_txt[sclmode],doloop?"yes":"no"); 
	post("");
}

#ifdef MAXMSP
V xspeed::m_assist(long msg, long arg, char *s)
{
	switch(msg) {
	case 1: //ASSIST_INLET:
		switch(arg) {
		case 0:
			sprintf(s,"Signal of playing speed"); break;
		case 1:
			sprintf(s,"Starting point"); break;
		case 2:
			sprintf(s,"Ending point"); break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		switch(arg) {
		case 0:
			sprintf(s,"Position signal"); break;
		case 1:
			sprintf(s,"Position currently played"); break;
		case 2:
			sprintf(s,"Starting point (rounded to sample)"); break;
		case 3:
			sprintf(s,"Ending point (rounded to sample)"); break;
		}
		break;
	}
}
#endif





