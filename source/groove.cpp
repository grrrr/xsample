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
	public xsample
{
//	FLEXT_HEADER_S(xgroove,xsample,setup)
	FLEXT_HEADER(xgroove,xsample)

public:
	xgroove(I argc,t_atom *argv);

#ifdef MAXMSP
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();

	virtual I m_set(I argc,t_atom *argv);

	virtual V m_start();
	virtual V m_stop();

	virtual V m_units(xs_unit mode = xsu__);

	virtual V m_pos(F pos);
	virtual V m_all();
	virtual V m_min(F mn);
	virtual V m_max(F mx);
	
	virtual V m_loop(BL lp) { doloop = lp; }
	
	virtual V m_reset();

protected:

	I outchns;
	BL doplay,doloop;
	D curpos;  // in samples

	outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { ToOutFloat(outmin,curmin*s2u); }
	V outputmax() { ToOutFloat(outmax,curmax*s2u); }
	
//	I bcnt,bframes;
//	F **bvecs;

private:
	static V setup(t_class *c);

	virtual V s_dsp();

	DEFSIGFUN(xgroove)	
	TMPLDEF V signal(I n,F *const *in,F *const *out);  // this is my dsp method

	FLEXT_CALLBACK_F(m_pos)
	FLEXT_CALLBACK(m_all)
	FLEXT_CALLBACK_F(m_min)
	FLEXT_CALLBACK_F(m_max)

	FLEXT_CALLBACK_B(m_loop)
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
	doplay(false),doloop(true),
	curpos(0),
	outchns(1)
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
	SetupInOut();

	FLEXT_ADDMETHOD(1,m_min);
	FLEXT_ADDMETHOD(2,m_max);
	FLEXT_ADDMETHOD_F(0,"min",m_min); 
	FLEXT_ADDMETHOD_F(0,"max",m_max);
	FLEXT_ADDMETHOD_F(0,"pos",m_pos);
	FLEXT_ADDMETHOD_(0,"all",m_all);

	FLEXT_ADDMETHOD_B(0,"loop",m_loop);


	outmin = GetOut(outchns+1);
	outmax = GetOut(outchns+2);
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
	curpos = pos?pos/s2u:0;
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}

V xgroove::m_all()
{
	xsample::m_all();
	outputmin();
	outputmax();
}

I xgroove::m_set(I argc,t_atom *argv)
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units(); 
	return r;
}

V xgroove::m_start() 
{ 
	m_refresh(); 
	doplay = true; 
	s_dsp();
}

V xgroove::m_stop() 
{ 
	doplay = false; 
	s_dsp();
}

V xgroove::m_reset()
{
	curpos = 0;
	xsample::m_reset();
}


TMPLDEF V xgroove::signal(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *speed = invecs[0];
	F *const *sig = outvecs;
	F *pos = outvecs[outchns];

	const I smin = curmin,smax = curmax;
	const I plen = curlen;
	D o = curpos;

	if(buf && doplay && plen > 0) {
		register I si = 0;

		if(interp == xsi_4p && plen >= 4) {
			// 4-point interpolation

			const I maxo = smax-1; // last sample in play region
			const I maxi = smax-3; // last position for straight interpolation

			for(I i = 0; i < n; ++i,++si) {	
				const F spd = *(speed++);  // must be first because the vector is reused for output!
				register I oint = (I)o;
				I ointm,oint1,oint2;

				if(doloop) {
					if(oint <= smin) { 
						if(oint < smin) { // position is before first sample -> roll over backwards
							o = fmod(o+smax,plen)+smin;
							// o is now somewhere at the end
							oint = (I)o;
							ointm = oint-1;
							oint1 = oint >= maxo?smin:oint+1;
							oint2 = oint1 >= maxo?smin+1:oint1+1;
						}
						else {
							// position is first simple
							ointm = smax-1; // last sample
							oint1 = oint+1;
							oint2 = oint1+1;
						}
					}
					else if(oint >= maxi) { 
						if(oint >= smax) { // position > last sample -> roll over forwards
							o = fmod(o,plen)+smin;
							// o is now somewhere at the beginning
							oint = (I)o;
							ointm = oint > smin?oint-1:maxo;
							oint1 = oint+1;
							oint2 = oint1+1;
						}
						else {
							ointm = oint-1;
							oint1 = oint >= maxo?smin:oint+1;
							oint2 = oint1 >= maxo?smin+1:oint1+1;
						}
					}
					else {
						ointm = oint-1;
						oint1 = oint+1;
						oint2 = oint1+1;
					}
				}
				else {
					// no looping
					
					if(oint <= smin) { 
						if(oint < smin) oint = smin,o = smin;
						// position is first simple
						ointm = smin; // first sample 
						oint1 = oint+1;
						oint2 = oint1+1;
					}
					else if(oint >= maxi) { 
						if(oint >= smax) oint = maxo,o = smax;
						ointm = oint-1;
						oint1 = oint >= maxo?maxo:oint+1;
						oint2 = oint1 >= maxo?maxo:oint1+1;
					}
					else {
						ointm = oint-1;
						oint1 = oint+1;
						oint2 = oint1+1;
					}
				}	

				register F frac = o-oint;
				
				register F *fa = buf->Data()+ointm*BCHNS;
				register F *fb = buf->Data()+oint*BCHNS;
				register F *fc = buf->Data()+oint1*BCHNS;
				register F *fd = buf->Data()+oint2*BCHNS;

				for(I ci = 0; ci < OCHNS; ++ci) {
					register const F cmb = fc[ci]-fb[ci];
					sig[ci][si] = fb[ci] + frac*( 
						cmb - 0.5f*(frac-1.) * ((fa[ci]-fd[ci]+3.0f*cmb)*frac + (fb[ci]-fa[ci]-cmb))
					);
				}
				
				*(pos++) = scale(o);
				o += spd;
			}
		}
		else if(interp == xsi_lin && plen >= 2) {
			// linear interpolation
		
			const I maxo = smax-1; // last sample in play region

			for(I i = 0; i < n; ++i,++si) {	
				const F spd = *(speed++);  // must be first because the vector is reused for output!
				register I oint = (I)o;

				if(doloop) {
					register I oint1;
					if(oint < smin) { // position is before first sample -> roll over backwards
						o = fmod(o+smax,plen)+smin;
						// o is now somewhere at the end
						oint = (I)o;
						oint1 = oint >= maxo?smin:oint+1;
					}
					else if(oint >= maxo) { 
						if(oint >= smax) { // position > last sample -> roll over forwards
							o = fmod(o,plen)+smin;
							// o is now somewhere at the beginning
							oint1 = (oint = (I)o)+1;
						}
						else
							oint1 = smin;
					}
					else oint1 = oint+1;
					
					register F frac = o-oint;
					register F *fp0 = buf->Data()+oint*BCHNS;
					register F *fp1 = buf->Data()+oint1*BCHNS;
					for(I ci = 0; ci < OCHNS; ++ci) 
						sig[ci][si] = fp0[ci] + frac*(fp1[ci]-fp0[ci]);
				}
				else {
					if(oint < smin) {
						// position is before first sample -> take the first sample
						o = smin;
						register const F *fp = buf->Data()+smin*BCHNS;
						for(I ci = 0; ci < OCHNS; ++ci) 
							sig[ci][si] = fp[ci];
					}
					else if(oint >= maxo) {
						// position >= last sample -> take the last sample
						if(o > smax) o = smax;
						register const F *fp = buf->Data()+maxo*BCHNS;
						for(I ci = 0; ci < OCHNS; ++ci) 
							sig[ci][si] = fp[ci]; 
					}
					else {
						register F frac = o-oint;
						register const F *fp0 = buf->Data()+oint*BCHNS;
						register const F *fp1 = fp0+BCHNS;
						for(I ci = 0; ci < OCHNS; ++ci) 
							sig[ci][si] = fp0[ci] + frac*(fp1[ci]-fp0[ci]);
					}
				}
				
				*(pos++) = scale(o);
				o += spd;
			}
		}
		else {
			// no interpolation
			if(doloop) {
				for(I i = 0; i < n; ++i,++si) {	
					const F spd = *(speed++);  // must be first because the vector is reused for output!

					// normalize offset
					if(o < smin) 
						o = fmod(o+smax,plen)+smin;
					else if(o >= smax) 
						o = fmod(o,plen)+smin;

					register const F *const fp = buf->Data()+(I)o*BCHNS;
					for(I ci = 0; ci < OCHNS; ++ci) 
						sig[ci][si] = fp[ci];

					*(pos++) = scale(o);
					o += spd;
				}
			}
			else {
				for(I i = 0; i < n; ++i,++si) {	
					const F spd = *(speed++);  // must be first because the vector is reused for output!
					
					register I oint = (I)o;
					if(oint < smin) 
						o = oint = smin;
					else if(oint >= smax)
						oint = smax-1,o = smax;
					
					register const F *const fp = buf->Data()+oint*BCHNS;
					for(I ci = 0; ci < OCHNS; ++ci)
						sig[ci][si] = fp[ci];

					*(pos++) = scale(o);
					o += spd;
				}
			}
		}

		// clear rest of output channels (if buffer has less channels)
		for(I ci = OCHNS; ci < outchns; ++ci) 
			for(I i = 0; i < n; ++i) 
				sig[ci][i] = 0;
	} 
	else {
		const F oscl = scale(o);
		for(I si = 0; si < n; ++si) {	
			*(pos++) = oscl;
			for(I ci = 0; ci < outchns; ++ci) sig[ci][si] = 0;
		}
	}
	
	// normalize and store current playing position
	if(o < smin) curpos = smin;
	else if(o >= smax) curpos = smax;
	else curpos = o;
}

V xgroove::s_dsp()
{
	switch(buf->Channels()*1000+outchns) {
		case 1000:
			// position only
			sigfun = SIGFUN(xgroove,signal,1,0);	break;
		case 1001:
			sigfun = SIGFUN(xgroove,signal,1,1);	break;
		case 1002:
			sigfun = SIGFUN(xgroove,signal,1,2);	break;
		case 2001: 
			sigfun = SIGFUN(xgroove,signal,2,1);	break;
		case 2002:
			sigfun = SIGFUN(xgroove,signal,2,2);	break;
		case 4001:
		case 4002:
		case 4003:
			sigfun = SIGFUN(xgroove,signal,4,-1);	break;
		case 4004:
			sigfun = SIGFUN(xgroove,signal,4,4);	break;
		default:
			sigfun = SIGFUN(xgroove,signal,-1,-1);
	}
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
	post("\tloop 0/1: switches looping off/on");
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
	post("");
} 

V xgroove::m_print()
{
	static const C *sclmode_txt[] = {"units","units in loop","buffer","loop"};
	static const C *interp_txt[] = {"off","4-point","linear"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("out channels = %i, samples/unit = %.3f, scale mode = %s",outchns,(F)(1./s2u),sclmode_txt[sclmode]); 
	post("loop = %s, interpolation = %s",doloop?"yes":"no",interp_txt[interp >= xsi_none && interp <= xsi_lin?interp:xsi_none]); 
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




