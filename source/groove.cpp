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


class xgroove:
	public xinter
{
//	FLEXT_HEADER_S(xgroove,xinter,setup)
	FLEXT_HEADER(xgroove,xinter)

public:
	xgroove(I argc,t_atom *argv);
	~xgroove();

#ifdef MAXMSP
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();

	virtual V m_units(xs_unit mode = xsu__);

	virtual BL m_reset();

	virtual V m_pos(F pos);
	virtual V m_all();
	virtual V m_min(F mn);
	virtual V m_max(F mx);
	
	V m_xzone(F xz) { xzone = xz < 0?0:xz/s2u; s_dsp(); }
	V m_xsymm(F xs) { xsymm = xs < -1?-1:(xs > 1?1:xs); }

	enum xs_loop {
		xsl__ = -1,  // don't change
		xsl_once = 0,xsl_loop,xsl_bidir
	};
	
	V m_loop(xs_loop lp = xsl__);
	
protected:

	xs_loop loopmode;
	D curpos;  // in samples
	I bidir;
	F xzone,xsymm;
	S *znbuf,*znmul;

	outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { ToOutFloat(outmin,curmin*s2u); }
	V outputmax() { ToOutFloat(outmax,curmax*s2u); }
	
	inline V setpos(F pos)
	{
		if(pos < curmin) pos = curmin;
		else if(pos > curmax) pos = curmax;
		curpos = pos;
	}

private:
//	static V setup(t_class *c);

	virtual V s_dsp();

	DEFSIGFUN(s_pos_off);
	DEFSIGFUN(s_pos_once);
	DEFSIGFUN(s_pos_loop);
	DEFSIGFUN(s_pos_loopzn);
	DEFSIGFUN(s_pos_bidir);

	DEFSIGCALL(posfun);
	virtual V m_signal(I n,S *const *in,S *const *out) 
	{ 
#ifdef MAXMSP // in max/msp the dsp tree is not rebuilt upon buffer resize
		if(buf->Update()) m_refresh();
#endif
		posfun(n,in,out); 
	}

	FLEXT_CALLBACK_F(m_pos)
	FLEXT_CALLBACK(m_all)
	FLEXT_CALLBACK_F(m_min)
	FLEXT_CALLBACK_F(m_max)
	
	FLEXT_CALLBACK_F(m_xzone)
	FLEXT_CALLBACK_F(m_xsymm)

	FLEXT_CALLBACK_1(m_loop,xs_loop)
};


FLEXT_LIB_TILDE_G("xgroove~",xgroove)

/*
V xgroove::setup(t_class *)
{
#ifndef PD
	post("loaded xgroove~ - part of xsample objects, version " XSAMPLE_VERSION " - (C) Thomas Grill, 2001-2002");
#endif
}
*/

xgroove::xgroove(I argc,t_atom *argv):
	loopmode(xsl_loop),curpos(0),
	xzone(0),xsymm(0),znbuf(NULL),znmul(NULL),
	bidir(1)
{
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
	AddOutSignal(outchns); // output
	AddOutSignal(); // position
	AddOutFloat(2); // play min & max	
	AddOutBang();  // loop bang
	SetupInOut();

	FLEXT_ADDMETHOD(1,m_min);
	FLEXT_ADDMETHOD(2,m_max);
	FLEXT_ADDMETHOD_F(0,"min",m_min); 
	FLEXT_ADDMETHOD_F(0,"max",m_max);
	FLEXT_ADDMETHOD_F(0,"pos",m_pos);
	FLEXT_ADDMETHOD_(0,"all",m_all);
	FLEXT_ADDMETHOD_B(0,"loop",m_loop);

	FLEXT_ADDMETHOD_F(0,"xzone",m_xzone);
	FLEXT_ADDMETHOD_F(0,"xsymm",m_xsymm);

	outmin = GetOut(outchns+1);
	outmax = GetOut(outchns+2);
	
	m_reset();
}

xgroove::~xgroove()
{
	if(znbuf) delete[] znbuf;
	if(znmul) delete[] znmul;
}

V xgroove::m_units(xs_unit mode)
{
	xsample::m_units(mode);
	
	m_sclmode();
	outputmin();
	outputmax();
}

V xgroove::m_min(F mn)
{
	xsample::m_min(mn);
	m_pos(curpos*s2u);
	outputmin();
}

V xgroove::m_max(F mx)
{
	xsample::m_max(mx);
	m_pos(curpos*s2u);
	outputmax();
}

V xgroove::m_pos(F pos)
{
	setpos(pos?pos/s2u:0);
}

V xgroove::m_all()
{
	xsample::m_all();
	outputmin();
	outputmax();
}

BL xgroove::m_reset()
{
	curpos = 0; 
	bidir = 1;
	return xsample::m_reset();
}

V xgroove::m_loop(xs_loop lp) 
{ 
	loopmode = lp; 
	bidir = 1;
	s_dsp();
}
	

V xgroove::s_pos_off(I n,S *const *invecs,S *const *outvecs)
{
	S *pos = outvecs[outchns];

	const F oscl = scale(curpos);
	for(I si = 0; si < n; ++si) pos[si] = oscl;
	
	playfun(n,&pos,outvecs); 
}

V xgroove::s_pos_once(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	const I smin = curmin,smax = curmax,plen = smax-smin; //curlen;

	if(buf && plen > 0) {
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!
			
			if(o >= smax) { o = smax; lpbang = true; }
			else if(o < smin) { o = smin; lpbang = true; }
			
			pos[i] = scale(o);
			o += spd;
		}
		// normalize and store current playing position
		setpos(o);

		playfun(n,&pos,outvecs); 
	} 
	else 
		s_pos_off(n,invecs,outvecs);
		
	if(lpbang) ToOutBang(outchns+3);
}

V xgroove::s_pos_loop(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	const I smin = curmin,smax = curmax,plen = smax-smin; //curlen;

	if(buf && plen > 0) {
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!

			// normalize offset
			if(o >= smax) {
				o = fmod(o-smin,plen)+smin;
				lpbang = true;
			}
			else if(o < smin) {
				o = fmod(o-smin,plen)+smax; 
				lpbang = true;
			}

			pos[i] = scale(o);
			o += spd;
		}
		// normalize and store current playing position
		setpos(o);

		playfun(n,&pos,outvecs); 
	} 
	else 
		s_pos_off(n,invecs,outvecs);
		
	if(lpbang) ToOutBang(outchns+3);
}

V xgroove::s_pos_loopzn(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	const I smin = curmin,smax = curmax,plen = smax-smin; //curlen;

	if(buf && plen > 0) {
	/*
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!

			// normalize offset
			if(o >= smax) {
				o = fmod(o-smin,plen)+smin;
				lpbang = true;
			}
			else if(o < smin) {
				o = fmod(o-smin,plen)+smax; 
				lpbang = true;
			}

			F o = scale(o);
			pos[i] = offs;
			o += spd;
		}
		// normalize and store current playing position
		setpos(o);

		playfun(n,&pos,outvecs); 
	*/
	} 
	else 
		s_pos_off(n,invecs,outvecs);
		
	if(lpbang) ToOutBang(outchns+3);
}

V xgroove::s_pos_bidir(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	const I smin = curmin,smax = curmax,plen = smax-smin; //curlen;

	if(buf && plen > 0) {
		register D o = curpos;
		register F bd = bidir;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!

			// normalize offset
			if(o >= smax) {
				o = smax-fmod(o-smin,plen); // mirror the position at smax
				bd = -bd;
				lpbang = true;
			}
			else if(o < smin) {
				o = smin-fmod(o-smin,plen); // mirror the position at smin
				bd = -bd;
				lpbang = true;
			}

			pos[i] = scale(o);
			o += spd*bd;
		}
		// normalize and store current playing position
		setpos(o);

		bidir = (I)bd;
		playfun(n,&pos,outvecs); 
	} 
	else 
		s_pos_off(n,invecs,outvecs);
		
	if(lpbang) ToOutBang(outchns+3);
}


V xgroove::s_dsp()
{
	if(znbuf) { 
		delete[] znbuf; znbuf = NULL; 
		delete[] znmul; znmul = NULL; 
	}

	if(doplay) {
		switch(loopmode) {
		case xsl_once: SETSIGFUN(posfun,SIGFUN(s_pos_once)); break;
		case xsl_loop: 
			if(xzone > 0) {
				znbuf = new S[Blocksize()];
				znmul = new S[Blocksize()];
				SETSIGFUN(posfun,SIGFUN(s_pos_loopzn)); 
			}
			else
				SETSIGFUN(posfun,SIGFUN(s_pos_loop)); 
			break;
		case xsl_bidir: SETSIGFUN(posfun,SIGFUN(s_pos_bidir)); break;
		}
	}
	else
		SETSIGFUN(posfun,SIGFUN(s_pos_off));
	xinter::s_dsp();
}




V xgroove::m_help()
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
	post("\tloop 0/1/2: sets looping to off/forward/bidirectional");
	post("\tinterp 0/1/2: set interpolation to off/4-point/linear");
	post("\tmin {unit}: set minimum playing point");
	post("\tmax {unit}: set maximum playing point");
	post("\tall: select entire buffer length");
	post("\tpos {unit}: set playing position (obeying the current scale mode)");
	post("\tbang/start: start playing");
	post("\tstop: stop playing");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tsclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("\txzone {unit}: length of loop crossfade zone");
	post("\txsymm -1...1: symmetry of crossfade zone");
	post("");
} 

V xgroove::m_print()
{
	static const C *sclmode_txt[] = {"units","units in loop","buffer","loop"};
	static const C *interp_txt[] = {"off","4-point","linear"};
	static const C *loop_txt[] = {"once","looping","bidir"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("out channels = %i, samples/unit = %.3f, scale mode = %s",outchns,(F)(1./s2u),sclmode_txt[sclmode]); 
	post("loop = %s, interpolation = %s",loop_txt[(I)loopmode],interp_txt[interp >= xsi_none && interp <= xsi_lin?interp:xsi_none]); 
	post("loop crossfade zone = %.3f, symmetry = %f",(F)(xzone*s2u),xsymm); 
	post("");
}

#ifdef MAXMSP
V xgroove::m_assist(long msg, long arg, char *s)
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
		if(arg < outchns)
			sprintf(s,"Audio signal channel %li",arg+1); 
		else
			switch(arg-outchns) {
			case 0:
				sprintf(s,"Position currently played"); break;
			case 1:
				sprintf(s,"Starting point (rounded to sample)"); break;
			case 2:
				sprintf(s,"Ending point (rounded to sample)"); break;
			}
		break;
	}
}
#endif






