/*

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2003 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
#include <math.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif


#define XZONE_TABLE 512


class xgroove:
	public xinter
{
	FLEXT_HEADER_S(xgroove,xinter,setup)

public:
	xgroove(I argc,const t_atom *argv);
	~xgroove();

	virtual BL Init();
		
	virtual V m_help();
	virtual V m_print();

	virtual V m_units(xs_unit mode = xsu__);

	virtual BL m_reset();

	virtual V m_pos(F pos);
	virtual V m_all();
	virtual V m_min(F mn);
	virtual V m_max(F mx);
	
	V m_xzone(F xz);
	V m_xsymm(F xz);
	V m_xshape(I argc = 0,const t_atom *argv = NULL);
	inline V ms_xshape(const AtomList &ret) { m_xshape(ret.Count(),ret.Atoms()); }
	V mg_xshape(AtomList &ret) const;
	V m_xkeep(BL k);

	enum xs_loop {
		xsl__ = -1,  // don't change
		xsl_once = 0,xsl_loop,xsl_bidir
	};
	
	V m_loop(xs_loop lp = xsl__);
	
protected:

	xs_loop loopmode;
	D curpos;  // in samples
	I bidir;

	F _xzone,xzone,xsymm;
	I xshape;
	F xshparam;
	F znmin,znmax;
	I xkeep;
	S **znbuf;
	S *znpos,*znmul,*znidx;
	I pblksz;

	inline V outputmin() { ToOutFloat(outchns+1,curmin*s2u); }
	inline V outputmax() { ToOutFloat(outchns+2,curmax*s2u); }
	
	inline V setpos(F pos)
	{
		if(pos < curmin) pos = curmin;
		else if(pos > curmax) pos = curmax;
		curpos = pos;
	}

	inline V mg_pos(F &v) const { v = curpos*s2u; }

private:
	static V setup(t_classid c);

	virtual V s_dsp();

	DEFSIGFUN(s_pos_off);
	DEFSIGFUN(s_pos_once);
	DEFSIGFUN(s_pos_loop);
	DEFSIGFUN(s_pos_loopzn);
	DEFSIGFUN(s_pos_bidir);

	DEFSIGCALL(posfun);

	DEFSTCALL(zonefun);

	V do_xzone();

	virtual V m_signal(I n,S *const *in,S *const *out) 
	{ 
		if(bufchk())
			posfun(n,in,out); 
		else
			zerofun(n,in,out);
	}

	FLEXT_CALLBACK_F(m_pos)
	FLEXT_CALLBACK_F(m_min)
	FLEXT_CALLBACK_F(m_max)
	FLEXT_CALLBACK(m_all)

	FLEXT_CALLSET_F(m_xzone)
	FLEXT_ATTRGET_F(_xzone)
	FLEXT_CALLSET_F(m_xsymm)
	FLEXT_ATTRGET_F(xsymm)
	FLEXT_CALLVAR_V(mg_xshape,ms_xshape)
	FLEXT_CALLSET_B(m_xkeep)
	FLEXT_ATTRGET_B(xkeep)

	FLEXT_CALLVAR_F(mg_pos,m_pos)
	FLEXT_CALLSET_F(m_min)
	FLEXT_CALLSET_F(m_max)
	FLEXT_CALLSET_E(m_loop,xs_loop)
	FLEXT_ATTRGET_E(loopmode,xs_loop)
};


FLEXT_LIB_DSP_V("xgroove~",xgroove)


V xgroove::setup(t_classid c)
{
	DefineHelp(c,"xgroove~");

	FLEXT_CADDMETHOD_(c,0,"all",m_all);
	FLEXT_CADDMETHOD(c,1,m_min);
	FLEXT_CADDMETHOD(c,2,m_max);

	FLEXT_CADDATTR_VAR(c,"min",mg_min,m_min); 
	FLEXT_CADDATTR_VAR(c,"max",mg_max,m_max);
	FLEXT_CADDATTR_VAR(c,"pos",mg_pos,m_pos);

	FLEXT_CADDATTR_VAR_E(c,"loop",loopmode,m_loop);

	FLEXT_CADDATTR_VAR(c,"xzone",_xzone,m_xzone);
	FLEXT_CADDATTR_VAR(c,"xsymm",xsymm,m_xsymm);
	FLEXT_CADDATTR_VAR(c,"xshape",mg_xshape,ms_xshape);
	FLEXT_CADDATTR_VAR(c,"xkeep",xkeep,m_xkeep);
}

xgroove::xgroove(I argc,const t_atom *argv):
	loopmode(xsl_loop),curpos(0),
	_xzone(0),xzone(0),xsymm(0.5),xkeep(0),pblksz(0),
	xshape(0),xshparam(1),
	znbuf(NULL),znmul(NULL),znidx(NULL),znpos(NULL),
	bidir(1)
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

	AddInSignal("Signal of playing speed"); // speed signal
	AddInFloat("Starting point"); // min play pos
	AddInFloat("Ending point"); // max play pos
	for(I ci = 0; ci < outchns; ++ci) {
		C tmp[30];
		STD::sprintf(tmp,"Audio signal channel %i",ci+1); 
		AddOutSignal(tmp); // output
	}
	AddOutSignal("Position currently played"); // position
	AddOutFloat("Starting point (rounded to frame)"); // play min 
	AddOutFloat("Ending point (rounded to frame)"); // play max
	AddOutBang("Bang on loop end/rollover");  // loop bang
	
	znbuf = new S *[outchns];
	for(I i = 0; i < outchns; ++i) znbuf[i] = new S[0];
	znpos = new S[0]; // don't know vector size yet -> m_dsp
	znidx = new S[0];
	znmul = new S[XZONE_TABLE+1];
	m_xshape();
}

xgroove::~xgroove()
{
	if(znbuf) {
		for(I i = 0; i < outchns; ++i) delete[] znbuf[i]; 
		delete[] znbuf;
	}

	if(znmul) delete[] znmul;
	if(znpos) delete[] znpos;
	if(znidx) delete[] znidx;
}

BL xgroove::Init()
{
	if(xinter::Init()) {
		m_reset();
		return true;
	}
	else
		return false;
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
	do_xzone();
	outputmin();
}

V xgroove::m_max(F mx)
{
	xsample::m_max(mx);
	m_pos(curpos*s2u);
	do_xzone();
	outputmax();
}

V xgroove::m_pos(F pos)
{
	setpos(pos?pos/s2u:0);
}

V xgroove::m_all()
{
	xsample::m_all();
	do_xzone();
	outputmin();
	outputmax();
}

BL xgroove::m_reset()
{
	curpos = 0; 
	bidir = 1;
	return xsample::m_reset();
}

V xgroove::m_xzone(F xz) 
{ 
	bufchk();
	_xzone = xz < 0?0:xz; 
	do_xzone();
	s_dsp(); 
}

V xgroove::m_xsymm(F xs) 
{ 
	if(xs < 0) 
		xsymm = -1;
	else if(xs <= 1) 
		xsymm = xs;
	else {
		post("%s - xsymm value out of range - set to center (0.5)",thisName());
		xsymm = 0.5;
	}
	do_xzone();
}

V xgroove::m_xshape(I argc,const t_atom *argv) 
{ 
	const F pi = 3.14159265358979f;
	xshape = 0;
	xshparam = 1;
	if(argc >= 1 && CanbeInt(argv[0])) xshape = GetAInt(argv[0]);
	if(argc >= 2 && CanbeFloat(argv[1])) {
		xshparam = GetAFloat(argv[1]);
		// clip to 0..1
		if(xshparam < 0) xshparam = 0;
		else if(xshparam > 1) xshparam = 1;
	}

	I i;
	switch(xshape) {
	case 1:
		for(i = 0; i <= XZONE_TABLE; ++i) 
			znmul[i] = sin(i*(pi/2./XZONE_TABLE))*xshparam+i*(1./XZONE_TABLE)*(1-xshparam);
		break;
	case 0:
	default:
		for(i = 0; i <= XZONE_TABLE; ++i) 
			znmul[i] = i*(1./XZONE_TABLE);
	}
}

V xgroove::mg_xshape(AtomList &ret) const
{ 
	ret(2);
	SetInt(ret[0],xshape);
	SetFloat(ret[1],xshparam);
}


V xgroove::m_xkeep(BL k) 
{ 
	xkeep = k; 
	do_xzone();
}

V xgroove::do_xzone()
{
	xzone = _xzone/s2u;
	I smin = curmin,smax = curmax,plen = smax-smin; //curlen;
	if(xsymm < 0) {
		// crossfade zone is inside the loop (-> loop is shorter than nominal!)
		if(xzone >= plen) xzone = plen-1;
		znmin = smin+xzone,znmax = smax-xzone;
	}
	else {
		// desired crossfade points
		znmin = smin+xzone*xsymm,znmax = smax+xzone*(xsymm-1);
		// extra space at beginning and end
		F o1 = znmin-xzone,o2 = buf->Frames()-(znmax+xzone);

		if(o1 < 0 || o2 < 0) { // or (o1*o2 < 0)
			if(o1+o2 < 0) {
				// must reduce crossfade/loop length
				if(!xkeep) {	
					// prefer preservation of cross-fade length
					if(xzone >= plen) // have to reduce cross-fade length
						xzone = plen-1;
					znmin = smin+xzone,znmax = smax-xzone;
				}
				else {	
					// prefer preservation of loop length
					znmin += o1,znmax -= o2;
					xzone = (buf->Frames()-znmax+znmin)/2;
				}
				smin = 0,plen = smax = buf->Frames();
			}
			else if(o1 < 0) {
				// min point is out of bounds (but enough space for mere shift)
				I i1 = (I)o1;
				smin -= i1,smax -= i1;
				znmin = smin+xzone*xsymm,znmax = smax+xzone*(xsymm-1);
			}
			else /* o2 < 0 */ { 
				// max point is out of bounds (but enough space for mere shift)
				I i2 = (I)o2;
				smin += i2,smax += i2;
				znmin = smin+xzone*xsymm,znmax = smax+xzone*(xsymm-1);
			}
		}
	}
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

	SetSamples(pos,n,curpos);
	
	playfun(n,&pos,outvecs); 

	SetSamples(pos,n,scale(curpos));
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
			
			pos[i] = o;
			o += spd;
		}
		// normalize and store current playing position
		setpos(o);

		playfun(n,&pos,outvecs); 

		arrscale(n,pos,pos);
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

			pos[i] = o;
			o += spd;
		}
		// normalize and store current playing position
		setpos(o);

		playfun(n,&pos,outvecs); 

		arrscale(n,pos,pos);
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
	const F xz = xzone,lmin = znmin,lmax = znmax;
	const F xf = (F)XZONE_TABLE/xz;

	if(buf && plen > 0) {
		BL inzn = false;
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

			if(o >= lmax) // in late cross-fade zone
				o -= lmax-smin;

			if(o < lmin) {
				// in early cross-fade zone
				register F inp = o-smin;
				znidx[i] = inp*xf;
				znpos[i] = lmax+inp;
				inzn = true;
			}
			else
				znidx[i] = XZONE_TABLE,znpos[i] = 0;

			pos[i] = o;
			o += spd;
		}
		// normalize and store current playing position
		setpos(o);

		playfun(n,&pos,outvecs); 

		arrscale(n,pos,pos);

		if(inzn) {
			// only if we were in cross-fade zone
			playfun(n,&znpos,znbuf); 
			
			arrscale(n,znidx,znpos,-XZONE_TABLE,-1);
			
			zonefun(znmul,0,XZONE_TABLE+1,n,1,1,&znidx,&znidx);
			zonefun(znmul,0,XZONE_TABLE+1,n,1,1,&znpos,&znpos);

			for(I o = 0; o < outchns; ++o) {
				F *ov = outvecs[o],*ob = znbuf[o];
				for(I i = 0; i < n; ++i,ov++,ob++)
					*ov = (*ov)*znidx[i]+(*ob)*znpos[i];
			}
		}
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

			pos[i] = o;
			o += spd*bd;
		}
		// normalize and store current playing position
		setpos(o);

		bidir = (I)bd;
		playfun(n,&pos,outvecs); 

		arrscale(n,pos,pos);
	} 
	else 
		s_pos_off(n,invecs,outvecs);
		
	if(lpbang) ToOutBang(outchns+3);
}


V xgroove::s_dsp()
{
	if(doplay) {
		switch(loopmode) {
		case xsl_once: SETSIGFUN(posfun,SIGFUN(s_pos_once)); break;
		case xsl_loop: 
			if(xzone > 0) {
				const I blksz = Blocksize();

				if(pblksz != blksz) {
					for(I o = 0; o < outchns; ++o) {
						delete[] znbuf[o]; 
						znbuf[o] = new S[blksz]; 
					}
				
					delete[] znpos; znpos = new S[blksz];
					delete[] znidx;	znidx = new S[blksz];

					pblksz = blksz;
				}

				SETSIGFUN(posfun,SIGFUN(s_pos_loopzn)); 

				// linear interpolation should be just ok for fade zone, no?
/*
				if(interp == xsi_4p) 
					switch(outchns) {
						case 1:	SETSTFUN(zonefun,TMPLSTF(st_play4,1,1)); break;
						case 2:	SETSTFUN(zonefun,TMPLSTF(st_play4,1,2)); break;
						case 4:	SETSTFUN(zonefun,TMPLSTF(st_play4,1,4)); break;
						default: SETSTFUN(zonefun,TMPLSTF(st_play4,1,-1));
					}
				else if(interp == xsi_lin) 
*/
					switch(outchns) {
						case 1:	SETSTFUN(zonefun,TMPLSTF(st_play2,1,1)); break;
						case 2:	SETSTFUN(zonefun,TMPLSTF(st_play2,1,2)); break;
						case 4:	SETSTFUN(zonefun,TMPLSTF(st_play2,1,4)); break;
						default: SETSTFUN(zonefun,TMPLSTF(st_play2,1,-1));
					}
/*
				else 
					switch(outchns) {
						case 1:	SETSTFUN(zonefun,TMPLSTF(st_play1,1,1)); break;
						case 2:	SETSTFUN(zonefun,TMPLSTF(st_play1,1,2)); break;
						case 4:	SETSTFUN(zonefun,TMPLSTF(st_play1,1,4)); break;
						default: SETSTFUN(zonefun,TMPLSTF(st_play1,1,-1));
					}
*/
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
#ifdef FLEXT_DEBUG
	post("compiled on " __DATE__ " " __TIME__);
#endif
	post("(C) Thomas Grill, 2001-2003");
#if FLEXT_SYS == FLEXT_SYS_MAX
	post("Arguments: %s [channels=1] [buffer]",thisName());
#else
	post("Arguments: %s [buffer]",thisName());
#endif
	post("Inlets: 1:Messages/Speed signal, 2:Min position, 3:Max position");
	post("Outlets: 1:Audio signal, 2:Position signal, 3:Min position (rounded), 4:Max position (rounded)");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset [name] / @buffer [name]: set buffer or reinit");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\treset: reset min/max playing points and playing offset");
	post("\tprint: print current settings");
	post("\t@loop 0/1/2: sets looping to off/forward/bidirectional");
	post("\t@interp 0/1/2: set interpolation to off/4-point/linear");
	post("\t@min {unit}: set minimum playing point");
	post("\t@max {unit}: set maximum playing point");
	post("\tall: select entire buffer length");
	post("\tpos {unit}: set playing position (obeying the current scale mode)");
	post("\tbang/start: start playing");
	post("\tstop: stop playing");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\t@units 0/1/2/3: set units to frames/buffer size/ms/s");
	post("\t@sclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("\txzone {unit}: length of loop crossfade zone");
	post("\txsymm -1,0...1: symmetry of crossfade zone inside/outside point");
	post("\txshape 0/1 [param 0...1]: shape of crossfading (linear/trig)");
	post("\txkeep 0/1: try to preserve xzone/loop length");
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
	post("out channels = %i, frames/unit = %.3f, scale mode = %s",outchns,(F)(1./s2u),sclmode_txt[sclmode]); 
	post("loop = %s, interpolation = %s",loop_txt[(I)loopmode],interp_txt[interp >= xsi_none && interp <= xsi_lin?interp:xsi_none]); 
	post("loop crossfade zone = %.3f",(F)(xzone*s2u)); 
	post("");
}

