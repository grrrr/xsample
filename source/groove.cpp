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
	virtual V m_min(F mn);
	virtual V m_max(F mx);
	
	virtual V m_loop(BL lp) { doloop = lp; }
	
	virtual V m_reset();

protected:

	I outchns;
	BL doplay,doloop;
	D curpos;  // in samples

	t_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { to_out_float(outmin,curmin*s2u); }
	V outputmax() { to_out_float(outmax,curmax*s2u); }
	
	I bcnt,bframes;
	F **bvecs;

private:
	virtual V m_dsp(I n,F *const *in,F *const *out);

	DEFSIGFUN(xgroove)	
	TMPLDEF V signal(I n,F *const *in,F *const *out);  // this is my dsp method

	FLEXT_CALLBACK(m_start)
	FLEXT_CALLBACK(m_stop)

	FLEXT_CALLBACK_1(m_pos,F)
	FLEXT_CALLBACK_1(m_min,F)
	FLEXT_CALLBACK_1(m_max,F)

	FLEXT_CALLBACK_B(m_loop)
};


FLEXT_TILDE_GIMME("xgroove~",xgroove)

V xgroove::cb_setup(t_class *c)
{
#ifndef PD
	post("loaded xgroove~ - part of xsample objects, version " XSAMPLE_VERSION " - (C) Thomas Grill, 2001-2002");
#endif

	FLEXT_ADDFLOAT_N(c,1,m_min);
	FLEXT_ADDFLOAT_N(c,2,m_max);
	FLEXT_ADDMETHOD_1(c,"min",m_min,F); 
	FLEXT_ADDMETHOD_1(c,"max",m_max,F);
	FLEXT_ADDMETHOD_1(c,"pos",m_pos,F);

	FLEXT_ADDBANG(c,m_start);
	FLEXT_ADDMETHOD(c,"start",m_start);
	FLEXT_ADDMETHOD(c,"stop",m_stop);

	FLEXT_ADDMETHOD_B(c,"loop",m_loop);
}

xgroove::xgroove(I argc,t_atom *argv):
	doplay(false),doloop(true),
	curpos(0)
{
	I argi = 0;
#ifdef MAXMSP
	if(argc > argi && ISFLINT(argv[argi])) {
		outchns = atom_getflintarg(argi,argc,argv);
		argi++;
	}
	else
#endif
	outchns = 1;

	if(argc > argi && ISSYMBOL(argv[argi])) {
		buf = new buffer(atom_getsymbolarg(argi,argc,argv),true);
		argi++;
	}
	else
		buf = new buffer(NULL,true);

	add_in_signal(); // speed signal
	add_in_float(2); // min & max play pos
	add_out_signal(outchns); // output
	add_out_signal(); // position
	add_out_float(2); // play min & max	
	setup_inout();

	outmin = get_out(outchns+1);
	outmax = get_out(outchns+2);
	
#ifdef PD
	m_loadbang();  // in PD loadbang is not called upon object creation
#endif
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
	curpos = pos/s2u;
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
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
}

V xgroove::m_stop() 
{ 
	doplay = false; 
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
		const I maxo = smax-3;
		register I si = 0;

		if(interp && plen >= 4) {
			for(I i = 0; i < n; ++i,++si) {	
				const F spd = *(speed++);

				// normalize offset
				if(o >= smax) 
					o = doloop?fmod(o-smin,plen)+smin:(D)smax-0.001;
				else if(o < smin) 
					o = doloop?fmod(o+(plen-smin),plen)+smin:smin;

				*(pos++) = scale(o);

				const I oint = (I)o;
				register F a,b,c,d;

				const F frac = o-oint;

				for(I ci = 0; ci < OCHNS; ++ci) {
					register const F *fp = buf->Data()+oint*BCHNS+ci;
					if (oint <= smin) {
						if(doloop) {
							// oint is always == smin
							a = buf->Data()[(smax-1)*BCHNS+ci];
							b = fp[0*BCHNS];
							c = fp[1*BCHNS];
							d = fp[2*BCHNS];
						}
						else {
							// offset is not normalized
							if(oint == smin) {
								a = b = fp[0*BCHNS];
								c = fp[1*BCHNS];
								d = fp[2*BCHNS];
							}
							else {
								a = b = c = d = buf->Data()[smin*BCHNS+ci];
							}
						}
					}
					else if(oint > maxo) {
						if(doloop) {
							a = fp[-1*BCHNS];
							b = fp[0*BCHNS];
							if(oint == maxo+1) {
								c = fp[1*BCHNS];
								d = buf->Data()[smin*BCHNS+ci];
							}
							else {
								c = buf->Data()[smin*BCHNS+ci];
								d = buf->Data()[(smin+1)*BCHNS+ci];
							}
						}
						else {
							// offset is not normalized
							if(oint == maxo+1) {
								a = fp[-1*BCHNS];
								b = fp[0*BCHNS];
								c = d = fp[1*BCHNS];	
							}
							else 
							if(oint == maxo+2) {
								a = fp[-1*BCHNS];
								b = c = d = fp[0*BCHNS];
							}
							else { 
								// >= (maxo+3 = smax-1)
								a = b = c = d = buf->Data()[(smax-1)*BCHNS+ci];
							}

						}
					}
					else {
						a = fp[-1*BCHNS];
						b = fp[0*BCHNS];
						c = fp[1*BCHNS];
						d = fp[2*BCHNS];
					}

					const F cmb = c-b;
					sig[ci][si] = b + frac*( 
						cmb - 0.5f*(frac-1.) * ((a-d+3.0f*cmb)*frac + (b-a-cmb))
					);
				}

				o += spd;
			}
		}
		else {
			if(doloop) {
				for(I i = 0; i < n; ++i,++si) {	
					F spd = *(speed++);

					// normalize offset
					if(o >= smax) 
						o = fmod(o,plen)+smin;
					else if(o < smin) 
						o = fmod(o+smax,plen)+smin;

					*(pos++) = scale(o);
					for(I ci = 0; ci < OCHNS; ++ci) 
						sig[ci][si] = buf->Data()[(I)o*BCHNS+ci];

					o += spd;
				}
			}
			else {
				for(I i = 0; i < n; ++i,++si) {	
					const F spd = *(speed++);

					*(pos++) = scale(o);

					const I oint = (I)o;
					if(oint < smin) {
						for(I ci = 0; ci < OCHNS; ++ci)
							sig[ci][si] = buf->Data()[smin*BCHNS+ci];
					}
					else if(oint >= smax) {
						for(I ci = 0; ci < OCHNS; ++ci)
							sig[ci][si] = buf->Data()[smax*BCHNS+ci];
					}
					else {
						for(I ci = 0; ci < OCHNS; ++ci)
							sig[ci][si] = buf->Data()[oint*BCHNS+ci];
					}

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
			for(I ci = 0; ci < outchns; ++ci)	sig[ci][si] = 0;
		}
	}
	
	curpos = o;
}

V xgroove::m_dsp(I /*n*/,F *const * /*insigs*/,F *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

	m_refresh();  

	switch(buf->Channels()*100+outchns) {
		case 101:
			sigfun = SIGFUN(xgroove,1,1);	break;
		case 102:
			sigfun = SIGFUN(xgroove,1,2);	break;
		case 201: 
			sigfun = SIGFUN(xgroove,2,1);	break;
		case 202:
			sigfun = SIGFUN(xgroove,2,2);	break;
		case 401:
		case 402:
		case 403:
			sigfun = SIGFUN(xgroove,4,0);	break;
		case 404:
			sigfun = SIGFUN(xgroove,4,4);	break;
		default:
			sigfun = SIGFUN(xgroove,0,0);
	}
}


V xgroove::m_help()
{
	post("%s - part of xsample objects, version " XSAMPLE_VERSION " compiled on " __DATE__ " " __TIME__,thisName());
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
	post("\tdspon 0/1: turn dsp calculation off/on");	
	post("\treset: reset min/max playing points and playing offset");
	post("\tprint: print current settings");
	post("\tloop 0/1: switches looping off/on");
	post("\tinterp 0/1: interpolation off/on");
	post("\tmin {unit}: set minimum playing point");
	post("\tmax {unit}: set maximum playing point");
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
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', frames = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("out channels = %i, samples/unit = %.3f, scale mode = %s",outchns,(F)(1./s2u),sclmode_txt[sclmode]); 
	post("loop = %s, interpolation = %s",doloop?"yes":"no",interp?"yes":"no"); 
	post("");
}

#ifdef MAXMSP
V xgroove::m_assist(long msg, long arg, char *s)
{
	switch(msg) {
	case 1: //ASSIST_INLET:
		switch(arg) {
		case 0:
			strcpy(s,"Signal of playing speed"); break;
		case 1:
			strcpy(s,"Starting point"); break;
		case 2:
			strcpy(s,"Ending point"); break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		if(arg < outchns)
			sprintf(s,"Audio signal channel %li",arg+1); 
		else
			switch(arg-outchns) {
			case 0:
				strcpy(s,"Position currently played"); break;
			case 1:
				strcpy(s,"Starting point (rounded to sample)"); break;
			case 2:
				strcpy(s,"Ending point (rounded to sample)"); break;
			}
		break;
	}
}
#endif




