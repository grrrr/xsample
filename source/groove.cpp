/*

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2004 Thomas Grill (xovo@gmx.net)
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
	inline V m_posmod(F pos) { setposmod(pos?pos/s2u:0); } // motivated by Tim Blechmann
	virtual V m_all();
	virtual V m_min(F mn);
	virtual V m_max(F mx);

	V ms_xfade(I xf);

	V ms_xzone(F xz);
	V mg_xzone(F &xz) { xz = _xzone*s2u; }

	V m_xshape(I argc = 0,const t_atom *argv = NULL);
	inline V ms_xshape(const AtomList &ret) { m_xshape(ret.Count(),ret.Atoms()); }
	V mg_xshape(AtomList &ret) const;

	enum xs_loop {
		xsl__ = -1,  // don't change
		xsl_once = 0,xsl_loop,xsl_bidir
	};

	enum xs_fade {
		xsf__ = -1,  // don't change
		xsf_keeplooppos = 0,xsf_keeplooplen,xsf_keepfade,xsf_inside
	};

	V m_loop(xs_loop lp = xsl__);
	
protected:

	xs_loop loopmode;
	D curpos;  // in samples
	I bidir;

	F _xzone,xzone;
    L znsmin,znsmax;
	xs_fade xfade;
	I xshape;
	F xshparam;
	D znmin,znmax;
	S **znbuf;
	S *znpos,*znmul,*znidx;
	I pblksz;

	inline V outputmin() { ToOutFloat(outchns+1,curmin*s2u); }
	inline V outputmax() { ToOutFloat(outchns+2,curmax*s2u); }
	
	inline V setpos(D pos)
	{
		if(pos < znsmin) curpos = znsmin;
		else if(pos > znsmax) curpos = znsmax;
		else curpos = pos;
	}

	inline V setposmod(D pos)
	{
		D p = pos-znsmin;
		if(p >= 0) curpos = znsmin+fmod(p,znsmax-znsmin);
		else curpos = znsmax+fmod(p,znsmax-znsmin);
	}

	inline V mg_pos(F &v) const { v = curpos*s2u; }

private:
	static V setup(t_classid c);

	virtual V s_dsp();

	DEFSIGFUN(s_pos_off);
	DEFSIGFUN(s_pos_once);
	DEFSIGFUN(s_pos_c_once);
	DEFSIGFUN(s_pos_a_once);
	DEFSIGFUN(s_pos_loop);
	DEFSIGFUN(s_pos_c_loop);
	DEFSIGFUN(s_pos_a_loop);
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
	FLEXT_CALLBACK_F(m_posmod)
	FLEXT_CALLBACK_F(m_min)
	FLEXT_CALLBACK_F(m_max)
	FLEXT_CALLBACK(m_all)

	FLEXT_CALLSET_I(ms_xfade)
	FLEXT_ATTRGET_I(xfade)
	FLEXT_CALLSET_F(ms_xzone)
	FLEXT_CALLGET_F(mg_xzone)
	FLEXT_CALLVAR_V(mg_xshape,ms_xshape)

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
	FLEXT_CADDMETHOD_(c,0,"posmod",m_posmod);

	FLEXT_CADDATTR_VAR_E(c,"loop",loopmode,m_loop);

	FLEXT_CADDATTR_VAR(c,"xfade",xfade,ms_xfade);
	FLEXT_CADDATTR_VAR(c,"xzone",mg_xzone,ms_xzone);
	FLEXT_CADDATTR_VAR(c,"xshape",mg_xshape,ms_xshape);
}

xgroove::xgroove(I argc,const t_atom *argv):
	loopmode(xsl_loop),curpos(0),
	_xzone(0),xzone(0),pblksz(0),
	xfade(xsf_keeplooppos),xshape(0),xshparam(1),
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
	
	// don't know vector size yet -> wait for m_dsp
	znbuf = new S *[outchns];
	for(I i = 0; i < outchns; ++i) znbuf[i] = NULL;
	znpos = NULL; 
	znidx = NULL;
	znmul = new S[XZONE_TABLE+1];

	m_xshape();
}

xgroove::~xgroove()
{
	if(znbuf) {
		for(I i = 0; i < outchns; ++i) if(znbuf[i]) delete[] znbuf[i]; 
		delete[] znbuf;
	}

	if(znpos) delete[] znpos;
	if(znidx) delete[] znidx;
	delete[] znmul;
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
	xsample::m_units(mode);  // calls bufchk()
	
	m_sclmode();  // calls bufchk() again.... \todo optimize that!!
	outputmin();
	outputmax();
}

V xgroove::m_min(F mn)
{
	xsample::m_min(mn);
	m_pos(curpos*s2u);
//	do_xzone();
	s_dsp();
	outputmin();
}

V xgroove::m_max(F mx)
{
	xsample::m_max(mx);
	m_pos(curpos*s2u);
//	do_xzone();
	s_dsp();
	outputmax();
}

V xgroove::m_pos(F pos)
{
	setpos(pos?pos/s2u:0);
}

V xgroove::m_all()
{
	xsample::m_all();
//	do_xzone();
	s_dsp();
	outputmin();
	outputmax();
}

BL xgroove::m_reset()
{
	curpos = 0; 
	bidir = 1;
	return xsample::m_reset();
}

V xgroove::ms_xfade(I xf) 
{ 
	xfade = (xs_fade)xf;
	//	do_xzone();
	s_dsp(); 
}

V xgroove::ms_xzone(F xz) 
{ 
	bufchk();
	_xzone = xz < 0?0:xz/s2u; 
//	do_xzone();
	s_dsp(); 
}

V xgroove::m_xshape(I argc,const t_atom *argv) 
{ 
	const F pi = 3.14159265358979f;
	xshape = 0;
	xshparam = 1;
	if(argc >= 1 && CanbeInt(argv[0])) xshape = GetAInt(argv[0]);
	if(argc >= 2 && CanbeFloat(argv[1])) {
		xshparam = GetAFloat(argv[1]);
/*
		// clip to 0..1
		if(xshparam < 0) xshparam = 0;
		else if(xshparam > 1) xshparam = 1;
*/
	}

	I i;
	switch(xshape) {
	case 1:
		// sine half wave
		for(i = 0; i <= XZONE_TABLE; ++i) 
			znmul[i] = sin(i*pi/(XZONE_TABLE*2))*xshparam+i*(1.f/XZONE_TABLE)*(1.f-xshparam);
		break;
	case 2:
		// sine full wave
		for(i = 0; i <= XZONE_TABLE; ++i) 
			znmul[i] = ((sin(i*(pi/XZONE_TABLE)-pi*0.5f)+1.f)*0.5f)*xshparam+i*(1.f/XZONE_TABLE)*(1.f-xshparam);
		break;
	case 0:
	default:
		// linear
		for(i = 0; i <= XZONE_TABLE; ++i) 
			znmul[i] = i*(1.f/XZONE_TABLE);
	}
}

V xgroove::mg_xshape(AtomList &ret) const
{ 
	ret(2);
	SetInt(ret[0],xshape);
	SetFloat(ret[1],xshparam);
}


V xgroove::do_xzone()
{
	if(!s2u) return; // this can happen if DSP is off

	xzone = _xzone; // make a copy for changing it

	if(xfade == xsf_inside) { // fade zone goes inside the loop -> loop gets shorter
		const L maxfd = (curmax-curmin)/2;
		if(xzone > maxfd) xzone = maxfd;

		znsmin = curmin,znsmax = curmax;
	}
	else if(xfade == xsf_keepfade) { 
		// try to keep fade zone
		// shifting of loop bounds may happen

		// restrict xzone to half of buffer
		const L maxfd = buf->Frames()/2;
		if(xzone > maxfd) xzone = maxfd;

		znsmin = curmin-xzone/2;
		znsmax = curmax+xzone/2;

		// check buffer limits and shift bounds if necessary
		if(znsmin < 0) {
			znsmax -= znsmin;
			znsmin = 0;
		}
		if(znsmax > buf->Frames())
			znsmax = buf->Frames();
	}
	else if(xfade == xsf_keeplooplen) { 
		// try to keep loop length
		// shifting of loop bounds may happen

		const L maxfd = buf->Frames()-(curmax-curmin);
		if(xzone > maxfd) xzone = maxfd;

		znsmin = curmin-xzone/2;
		znsmax = curmax+xzone/2;

		// check buffer limits and shift bounds if necessary
		// both cases can't happen because of xzone having been limited above
		if(znsmin < 0) {
			znsmax -= znsmin;
			znsmin = 0;
		}
		else if(znsmax > buf->Frames()) {
			znsmin -= znsmax-buf->Frames();
			znsmax = buf->Frames();
		}
	}
	else if(xfade == xsf_keeplooppos) { 
		// try to keep loop position and length

		znsmin = curmin-xzone/2;
		znsmax = curmax+xzone/2;

		L ovr = znsmax-buf->Frames();
		if(-znsmin > ovr) ovr = -znsmin;
		if(ovr > 0) {
			znsmin += ovr;
			znsmax -= ovr;
			xzone -= ovr*2;
		}
	}

	znmin = znsmin+xzone;
	znmax = znsmax-xzone;

	FLEXT_ASSERT(znsmin <= znsmax && (znsmax-znsmin) >= xzone*2);
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

	const D smin = curmin,smax = curmax,plen = smax-smin;

	if(buf && plen > 0) {
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!
			
			if(!(o < smax)) { o = smax; lpbang = true; }
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

// \TODO optimize that for spd = const!
V xgroove::s_pos_c_once(I n,S *const *invecs,S *const *outvecs)
{
	const S spd = *invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	const D smin = curmin,smax = curmax,plen = smax-smin;

	if(buf && plen > 0) {
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			if(!(o < smax)) { o = smax; lpbang = true; }
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

V xgroove::s_pos_a_once(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	if(speed[0] == speed[n-1]) 
		// assume constant speed
		s_pos_c_once(n,invecs,outvecs);
	else
		s_pos_once(n,invecs,outvecs);
}

V xgroove::s_pos_loop(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

#ifdef __VEC__
	// prefetch cache
	vec_dst(speed,GetPrefetchConstant(1,n>>2,0),0);
#endif

	const D smin = curmin,smax = curmax,plen = smax-smin;

	if(buf && plen > 0) {
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!

			// normalize offset
			if(!(o < smax)) {  // faster than o >= smax
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
		
#ifdef __VEC__
	vec_dss(0);
#endif

	if(lpbang) ToOutBang(outchns+3);
}

// \TODO optimize that for spd = const!
V xgroove::s_pos_c_loop(I n,S *const *invecs,S *const *outvecs)
{
	const S spd = *invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	const D smin = curmin,smax = curmax,plen = smax-smin;

	if(buf && plen > 0) {
		register D o = curpos;

		for(I i = 0; i < n; ++i) {	
			// normalize offset
			if(!(o < smax)) {  // faster than o >= smax
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

V xgroove::s_pos_a_loop(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	if(speed[0] == speed[n-1]) 
		// assume constant speed
		s_pos_c_loop(n,invecs,outvecs);
	else
		s_pos_loop(n,invecs,outvecs);
}

V xgroove::s_pos_loopzn(I n,S *const *invecs,S *const *outvecs)
{
	const S *speed = invecs[0];
	S *pos = outvecs[outchns];
	BL lpbang = false;

	FLEXT_ASSERT(xzone);

	const F xz = xzone,xf = (F)XZONE_TABLE/xz;
	const D lmin = znmin,lmax = znmax,lsh = lmax-lmin+xz;

    // adapt the playing bounds to the current cross-fade zone
    const L smin = znsmin,smax = znsmax,plen = smax-smin;

	if(buf && plen > 0) {
		BL inzn = false;
		register D o = curpos;

 		for(I i = 0; i < n; ++i) {	
 			// \TODO: exploit relationships: smin <= lmin, smax >= lmax
 		
			// normalize offset
			if(!(o < smax)) {
				o = fmod(o-smin,plen)+smin;
				lpbang = true;
			}
            else if(o < smin) {
				o = fmod(o-smin,plen)+smax; 
				lpbang = true;
			}

            if(o >= lmax) {
                // in late cross-fade zone
				o -= lsh;
			}

            // now: smin <= o < smax
			if(o < lmin) {
				// in early cross-fade zone
				register F inp = xz+(F)(o-lmin);  // 0 <= inp < xz
				znidx[i] = inp*xf;
				znpos[i] = lmax+inp;
				inzn = true;
			}
			else
				znidx[i] = XZONE_TABLE,znpos[i] = 0;

			const S spd = speed[i];  // must be first because the vector is reused for output!
			pos[i] = o;
			o += spd;
		}

		// normalize and store current playing position
		if(o < znsmin) o += plen;
		setpos(o);

		// calculate samples (1st voice)
		playfun(n,&pos,outvecs); 

		if(inzn) {
			// only if we are in cross-fade zone
			
			// calculate samples in loop zone (2nd voice)
			playfun(n,&znpos,znbuf); 
			
			// calculate counterpart in loop fade
			arrscale(n,znidx,znpos,XZONE_TABLE,-1);
			
			// calculate fade coefficients
			zonefun(znmul,0,XZONE_TABLE+1,n,1,1,&znidx,&znidx);
			zonefun(znmul,0,XZONE_TABLE+1,n,1,1,&znpos,&znpos);

			// mix voices for all channels
			for(I o = 0; o < outchns; ++o) {
				F *ov = outvecs[o],*ob = znbuf[o];
				for(I i = 0; i < n; ++i,ov++,ob++)
					*ov = (*ov)*znidx[i]+(*ob)*znpos[i];
			}
		}
	
		// rescale position vector
		arrscale(n,pos,pos);
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

	const I smin = curmin,smax = curmax,plen = smax-smin;

	if(buf && plen > 0) {
		register D o = curpos;
		register F bd = bidir;

		for(I i = 0; i < n; ++i) {	
			const S spd = speed[i];  // must be first because the vector is reused for output!

			// normalize offset
			if(!(o < smax)) {
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
            // xzone might not be set yet (is done in do_xzone() )
			do_xzone(); // recalculate (s2u may have been 0 before)

			if(xzone > 0) {
				const I blksz = Blocksize();

				if(pblksz != blksz) {
					for(I o = 0; o < outchns; ++o) {
						if(znbuf[o]) delete[] znbuf[o]; 
						znbuf[o] = new S[blksz]; 
					}
				
					if(znpos) delete[] znpos; 
					znpos = new S[blksz];
					if(znidx) delete[] znidx;
					znidx = new S[blksz];

					pblksz = blksz;
				}

				SETSIGFUN(posfun,SIGFUN(s_pos_loopzn)); 

				// linear interpolation should be just ok for fade zone, no?
				switch(outchns) {
					case 1:	SETSTFUN(zonefun,TMPLSTF(st_play2,1,1)); break;
					case 2:	SETSTFUN(zonefun,TMPLSTF(st_play2,1,2)); break;
					case 4:	SETSTFUN(zonefun,TMPLSTF(st_play2,1,4)); break;
					default: SETSTFUN(zonefun,TMPLSTF(st_play2,1,-1));
				}
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
	post("(C) Thomas Grill, 2001-2004");
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
	post("\tposmod {unit}: set playing position (modulo into min/max range)");
	post("\tbang/start: start playing");
	post("\tstop: stop playing");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\t@units 0/1/2/3: set units to frames/buffer size/ms/s");
	post("\t@sclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("\t@xzone {unit}: length of loop crossfade zone");
	post("\t@xsymm -1,0...1: symmetry of crossfade zone inside/outside point");
	post("\t@xshape 0/1 [param 0...1]: shape of crossfading (linear/trig)");
	post("\t@xkeep 0/1: try to preserve xzone/loop length");
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

