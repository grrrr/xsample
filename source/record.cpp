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


class xrecord:
	public xsample
{
	FLEXT_HEADER_S(xrecord,xsample,setup)

public:
	xrecord(I argc,const t_atom *argv);
	
	virtual BL Init();
		
	virtual V m_help();
	virtual V m_print();
	
	virtual I m_set(I argc,const t_atom *argv);

	virtual V m_pos(F pos);
	virtual V m_all();
	virtual V m_start();
	virtual V m_stop();

	virtual BL m_reset();

	virtual V m_units(xs_unit md = xsu__);
	virtual V m_min(F mn);
	virtual V m_max(F mx);

	inline V m_append(BL app) { if(!(appmode = app)) m_pos(0); }

	virtual V m_draw(I argc,const t_atom *argv);	

protected:
	I inchns;
	BL sigmode,appmode;
	F drintv;

	BL dorec,doloop,mixmode;
	L curpos;  // in samples

	inline V outputmin() { ToOutFloat(1,curmin*s2u); }
	inline V outputmax() { ToOutFloat(2,curmax*s2u); }

	inline V mg_pos(F &v) const { v = curpos*s2u; }
	
private:
	static V setup(t_classid c);

	virtual V s_dsp();

	TMPLSIGFUN(s_rec);

	DEFSIGCALL(recfun);
	virtual V m_signal(I n,S *const *in,S *const *out) 
	{ 
		bufchk();
		recfun(n,in,out); 
	}

	FLEXT_CALLVAR_F(mg_pos,m_pos)
	FLEXT_CALLBACK(m_all)
	FLEXT_CALLSET_F(m_min)
	FLEXT_CALLSET_F(m_max)
	FLEXT_CALLBACK_F(m_min)
	FLEXT_CALLBACK_F(m_max)

	FLEXT_ATTRVAR_B(doloop)
	FLEXT_ATTRVAR_B(mixmode)
	FLEXT_ATTRVAR_B(sigmode)
	FLEXT_CALLSET_B(m_append)
	FLEXT_ATTRGET_B(appmode)

	FLEXT_CALLBACK_V(m_draw)
};


FLEXT_LIB_DSP_V("xrecord~",xrecord)


V xrecord::setup(t_classid c)
{
	DefineHelp(c,"xrecord~");

	FLEXT_CADDATTR_VAR(c,"pos",mg_pos,m_pos);
	FLEXT_CADDATTR_VAR(c,"min",mg_min,m_min);
	FLEXT_CADDATTR_VAR(c,"max",mg_max,m_max);
	FLEXT_CADDMETHOD_(c,0,"all",m_all);
	
	FLEXT_CADDMETHOD_(c,0,"draw",m_draw);
	
	FLEXT_CADDATTR_VAR1(c,"loop",doloop);
	FLEXT_CADDATTR_VAR1(c,"mixmode",mixmode);
	FLEXT_CADDATTR_VAR1(c,"sigmode",sigmode);
	FLEXT_CADDATTR_VAR(c,"append",appmode,m_append);
}

xrecord::xrecord(I argc,const t_atom *argv):
	dorec(false),
	sigmode(false),mixmode(false),
	appmode(true),doloop(false),
	drintv(0),
	inchns(1)
{
	I argi = 0;
#if FLEXT_SYS == FLEXT_SYS_MAX
	if(argc > argi && CanbeInt(argv[argi])) {
		inchns = GetAInt(argv[argi]);
		argi++;
	}
#endif

	if(argc > argi && IsSymbol(argv[argi])) {
		buf = new buffer(GetSymbol(argv[argi]),true);
		argi++;

#if FLEXT_SYS == FLEXT_SYS_MAX
		// oldstyle command line?
		if(argi == 1 && argc == 2 && CanbeInt(argv[argi])) {
			inchns = GetAInt(argv[argi]);
			argi++;
			post("%s: old style command line detected - please change to '%s [channels] [buffer]'",thisName(),thisName()); 
		}
#endif
	}
	else
		buf = new buffer(NULL,true);

	for(I ci = 0; ci < inchns; ++ci) {
		C tmp[40];
		STD::sprintf(tmp,ci == 0?"Messages/audio channel %i":"Audio channel %i",ci+1);
		AddInSignal(tmp);  // audio signals
	}
	AddInSignal("On/Off/Fade/Mix signal (0..1)"); // on/off signal
	AddInFloat("Starting point of recording");  // min 
	AddInFloat("Ending point of recording");  // max
	AddOutSignal("Current position of recording");  // pos signal
	AddOutFloat("Starting point (rounded to frame)"); // min 
	AddOutFloat("Ending point (rounded to frame)"); // max
	AddOutBang("Bang on loop end/rollover");  // loop bang

	FLEXT_ADDMETHOD(inchns+1,m_min);
	FLEXT_ADDMETHOD(inchns+2,m_max);
}


BL xrecord::Init()
{
	if(xsample::Init()) {
		m_reset();
		return true;
	}
	else
		return false;
}
		
V xrecord::m_units(xs_unit mode)
{
	xsample::m_units(mode);

	m_sclmode();
	outputmin();
	outputmax();
}

V xrecord::m_min(F mn)
{
	xsample::m_min(mn);
	m_pos(curpos*s2u);
	outputmin();
}

V xrecord::m_max(F mx)
{
	xsample::m_max(mx);
	m_pos(curpos*s2u);
	outputmax();
}

V xrecord::m_all()
{
	xsample::m_all();
	outputmin();
	outputmax();
}

V xrecord::m_pos(F pos)
{
	curpos = pos?(L)(pos/s2u+.5):0;

	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}


I xrecord::m_set(I argc,const t_atom *argv)
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units();
	return r;
}

V xrecord::m_start() 
{ 
	if(!sigmode && !appmode) m_pos(0); 
	m_refresh(); 
	dorec = true; 
	buf->SetRefrIntv(drintv);
	s_dsp();
}

V xrecord::m_stop() 
{ 
	dorec = false; 
	buf->Dirty(true);
	buf->SetRefrIntv(0);
	s_dsp();
}

BL xrecord::m_reset()
{
	curpos = 0;
	return xsample::m_reset();
}

V xrecord::m_draw(I argc,const t_atom *argv)
{
	if(argc >= 1) {
		drintv = GetInt(argv[0]);
		if(dorec) buf->SetRefrIntv(drintv);
	}
	else
		buf->Dirty(true);
}
	
	
TMPLDEF V xrecord::s_rec(I n,S *const *invecs,S *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),ICHNS,inchns);

	const S *const *sig = invecs;
	register I si = 0;
	const S *on = invecs[inchns];
	S *pos = outvecs[0];

	BL lpbang = false;
	register const F pf = sclmul;
	register L o = curpos;
	
	if(o < curmin) o = curmin;

//	if(buf && dorec && curlen > 0) {
	if(buf && dorec && curmax > curmin) {
		while(n) {
			L ncur = curmax-o; // at max to buffer or recording end

			if(ncur <= 0) {	// end of buffer
				if(doloop) { 
					o = curmin;
//					ncur = curlen;
					ncur = curmax-o;
				}
				else 
					m_stop(); // loop expired;
					
				lpbang = true;
			}

			if(!dorec) break;

			if(ncur > n) ncur = n;
			
			register I i;
			register S *bf = buf->Data()+o*BCHNS;
			register F p = scale(o);

			if(sigmode) {
				if(appmode) {
					// append to current position
				
					if(!mixmode) {
						for(i = 0; i < ncur; ++i,++si) {	
							if(*(on++) >= 0) {
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = sig[ci][si];	
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else 
								*(pos++) = p;
						}
					}
					else {
						for(i = 0; i < ncur; ++i,++si) {	
							register const S g = *(on++);
							if(g >= 0) {
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = bf[ci]*(1.-g)+sig[ci][si]*g;
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else 
								*(pos++) = p;
						}
					}
				}
				else {  
					// don't append
					if(!mixmode) {
						for(i = 0; i < ncur; ++i,++si) {	
							if(*(on++) >= 0)
							{ 	
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = sig[ci][si];	
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else {
								*(pos++) = p = scale(o = 0);
								bf = buf->Data();
							}
						}
					}
					else {
						for(i = 0; i < ncur; ++i,++si) {	
							register const S g = *(on++);
							if(g >= 0) {
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = bf[ci]*(1.-g)+sig[ci][si]*g;
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else {
								*(pos++) = p = scale(o = 0);
								bf = buf->Data();
							}
						}
					}
				}
			}
			else { 
				// message mode
				
				// Altivec optimization for that!
				if(!mixmode) {
					for(int ci = 0; ci < ICHNS; ++ci) {	
						register S *b = bf+ci;
						register const F *s = sig[ci];
						for(i = 0; i < ncur; ++i,b += BCHNS,++s) *b = *s;	
					}
					si += ncur;
				}
				else {
					for(i = 0; i < ncur; ++i,++si) {	
						register const S w = *(on++);
						for(int ci = 0; ci < ICHNS; ++ci)
							bf[ci] = bf[ci]*(1.-w)+sig[ci][si]*w;
						bf += BCHNS;
					}
				}
				for(i = 0; i < ncur; ++i) {
					*(pos++) = p,p += pf,++o;
				}
			}

			n -= ncur;
		} 
		curpos = o;
		
		buf->Dirty(); 
	}

	if(n) {
		register F p = scale(o);
		while(n--) *(pos++) = p;
	}
	
	if(lpbang) ToOutBang(3);
}

V xrecord::s_dsp()
{
	switch(buf->Channels()*1000+inchns) {
	case 1001:	SETSIGFUN(recfun,TMPLFUN(s_rec,1,1));	break;
	case 1002:	SETSIGFUN(recfun,TMPLFUN(s_rec,1,2));	break;
	case 2001:	SETSIGFUN(recfun,TMPLFUN(s_rec,2,1));	break;
	case 2002:	SETSIGFUN(recfun,TMPLFUN(s_rec,2,2));	break;
	case 4001:
	case 4002:
	case 4003:	SETSIGFUN(recfun,TMPLFUN(s_rec,4,-1));	break;
	case 4004:	SETSIGFUN(recfun,TMPLFUN(s_rec,4,4));	break;
	default:	SETSIGFUN(recfun,TMPLFUN(s_rec,-1,-1));	break;
	}
}




V xrecord::m_help()
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
	post("Inlets: 1:Messages/Audio signal, 2:Trigger signal, 3:Min point, 4: Max point");
	post("Outlets: 1:Position signal, 2:Min point, 3:Max point");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset [name]: set buffer or reinit");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\treset: reset min/max recording points and recording offset");
	post("\tprint: print current settings");
	post("\t@sigmode 0/1: specify message or signal triggered recording");
	post("\t@append 0/1: reset recording position or append to current position");
	post("\t@loop 0/1: switches looping off/on");
	post("\t@mixmode 0/1: specify if audio signal should be mixed in");
	post("\tmin {unit}: set minimum recording point");
	post("\tmax {unit}: set maximum recording point");
	post("\tall: select entire buffer length");
	post("\tpos {unit}: set recording position (obeying the current scale mode)");
	post("\tbang/start: start recording");
	post("\tstop: stop recording");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\t@units 0/1/2/3: set units to frames/buffer size/ms/s");
	post("\t@sclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("\tdraw [{float}]: redraw buffer immediately (arg omitted) or periodic (in ms)");
	post("");
}

V xrecord::m_print()
{
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("in channels = %i, frames/unit = %.3f, scale mode = %s",inchns,(F)(1./s2u),sclmode_txt[sclmode]); 
	post("sigmode = %s, append = %s, loop = %s, mixmode = %s",sigmode?"yes":"no",appmode?"yes":"no",doloop?"yes":"no",mixmode?"yes":"no"); 
	post("");
}

