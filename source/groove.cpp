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


//#define DEBUG 

class xgroove_obj:
	public xs_obj
{
	FLEXT_HEADER(xgroove_obj,xs_obj)

public:
	xgroove_obj(I argc,t_atom *argv);
	~xgroove_obj();

#ifdef MAXMSP
	virtual V m_loadbang() { m_refresh(); }
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
	F **outvecs;
	BL doplay,doloop;
	D curpos;  // in samples

	_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { outlet_float(outmin,curmin*s2u); }
	V outputmax() { outlet_float(outmax,curmax*s2u); }

#ifdef TMPLOPT
	template <I _BCHNS_,I _OCHNS_>
#endif
	V signal(I n,const F *speed,F *pos);  // this is the dsp method
	
private:
	virtual V m_dsp(t_signal **sp);

	static V cb_start(t_class *c) { thisObject(c)->m_start(); }
	static V cb_stop(t_class *c) { thisObject(c)->m_stop(); }

#ifdef TMPLOPT
	template <I _BCHNS_,I _OCHNS_>
#endif
	static t_int *dspmeth(t_int *w);

	static V cb_pos(V *c,F pos) { thisObject(c)->m_pos(pos); }
	static V cb_min(V *c,F mn) { thisObject(c)->m_min(mn); }
	static V cb_max(V *c,F mx) { thisObject(c)->m_max(mx); }

	static V cb_loop(V *c,FI lp) { thisObject(c)->m_loop(lp != 0); }
};



FLEXT_NEW_WITH_GIMME("xgroove~",xgroove_obj)

V xgroove_obj::cb_setup(t_class *c)
{
	add_float1(c,cb_min);
	add_float2(c,cb_max);

	add_method1(c,cb_loop, "loop", A_FLINT);	
	add_method1(c,cb_min, "min", A_FLOAT);	
	add_method1(c,cb_max, "max", A_FLOAT);	

	add_bang(c,cb_start);	
	add_method0(c,cb_start, "start");	
	add_method0(c,cb_stop, "stop");	
	add_float(c,cb_pos);	
	add_method1(c,cb_pos, "pos", A_FLOAT);	
}

xgroove_obj::xgroove_obj(I argc,t_atom *argv):
	doplay(false),doloop(true),
	curpos(0),
	outvecs(NULL)
{
#ifdef DEBUG
	if(argc < 1) {
		post("%s - Warning: no buffer defined",thisName());
	} 
#endif
	
#ifdef MAXMSP
	outchns = argc >= 2?atom_getflintarg(1,argc,argv):1;
#else
	outchns = 1;
#endif

/*
#ifdef PD	
    inlet_new(x_obj, &x_obj->ob_pd, &s_float, gensym("ft1"));  // min play pos
    inlet_new(x_obj, &x_obj->ob_pd, &s_float, gensym("ft2"));  // max play pos
    
	int ci;
	for(ci = 0; ci < outchns; ++ci) newout_signal(x_obj); // output
	newout_signal(x_obj); // position
	outmin = newout_float(x_obj); // play min
	outmax = newout_float(x_obj); // play max
#elif defined(MAXMSP)
	// set up inlets and outlets in reverse
	floatin(x_obj,2);  // max play pos
	floatin(x_obj,1);  // min play pos
	dsp_setup(x_obj,1); // speed sig
	
	outmax = newout_float(x_obj); // play max
	outmin = newout_float(x_obj); // play min
	newout_signal(x_obj); // position signal
	int ci;
	for(ci = 0; ci < outchns; ++ci) newout_signal(x_obj); // output
#endif
*/
	Inlet_signal(); // speed signal
	Inlet_float(2); // min & max play pos
	Outlet_signal(outchns); // output
	Outlet_signal(); // position
	Outlet_float(2); // play min & max	
	SetupInOut();

	outmin = Outlet(outchns+1);
	outmax = Outlet(outchns+2);
	
	buf = new buffer(argc >= 1?atom_getsymbolarg(0,argc,argv):NULL);
	// in max loadbang does the init again
	m_reset();
}

xgroove_obj::~xgroove_obj()
{
	if(buf) delete buf;
	if(outvecs) delete[] outvecs;
}


V xgroove_obj::m_units(xs_unit mode)
{
	xs_obj::m_units(mode);
	
	m_sclmode();
	outputmin();
	outputmax();
}

V xgroove_obj::m_min(F mn)
{
	xs_obj::m_min(mn);
	m_pos(curpos);
	outputmin();
}

V xgroove_obj::m_max(F mx)
{
	xs_obj::m_max(mx);
	m_pos(curpos);
	outputmax();
}


V xgroove_obj::m_pos(F pos)
{
	curpos = pos/s2u;
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}

I xgroove_obj::m_set(I argc,t_atom *argv)
{
	I r = xs_obj::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units(); 
	return r;
}

V xgroove_obj::m_start() 
{ 
	m_refresh(); 
	doplay = true; 
}

V xgroove_obj::m_stop() 
{ 
	doplay = false; 
}

V xgroove_obj::m_reset()
{
	curpos = 0;
	xs_obj::m_reset();
}



#ifdef TMPLOPT
template <int _BCHNS_,int _OCHNS_>
t_int *xgroove_obj::dspmeth(t_int *w) 
{ 
	((xgroove_obj *)w[1])->signal<_BCHNS_,_OCHNS_>((I)w[2],(const F *)w[3],(F *)w[4]); 
	return w+5;
}
#else
t_int *xgroove_obj::dspmeth(t_int *w) 
{ 
	((xgroove_obj *)w[1])->signal((I)w[2],(const F *)w[3],(F *)w[4]); 
	return w+5;
}
#endif

#ifndef MIN
#define MIN(x,y) ((x) < (y)?(x):(y))
#endif

#ifdef TMPLOPT
template <int _BCHNS_,int _OCHNS_>
#endif
V xgroove_obj::signal(I n,const F *speed,F *pos)
{
	if(enable) {    
#ifdef TMPLOPT
		const I BCHNS = _BCHNS_ == 0?buf->Channels():_BCHNS_;
		const I OCHNS = _OCHNS_ == 0?MIN(outchns,BCHNS):MIN(_OCHNS_,BCHNS);
#else
		const I BCHNS = buf->Channels();
		const I OCHNS = MIN(outchns,BCHNS);
#endif
		F **sig = outvecs;
		register I si = 0;
	
		const I smin = curmin,smax = curmax;
		const I plen = curlen;
		D o = curpos;

		if(buf && doplay && plen > 0) {
			const I maxo = smax-3;

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
		} 
		else {
			const F oscl = scale(o);
			while(n--) {	
				*(pos++) = oscl;
				for(I ci = 0; ci < OCHNS; ++ci)	sig[ci][si] = 0;
				++si;
			}
		}
		
		curpos = o;
	}   
}

V xgroove_obj::m_dsp(t_signal **sp)
{
	m_refresh();  // m_dsp hopefully called at change of sample rate ?!

	if(outvecs) delete[] outvecs;
	outvecs = new F *[buf->Channels()];
	for(I ci = 0; ci < buf->Channels(); ++ci)
		outvecs[ci] = sp[1+ci%outchns]->s_vec;

#ifdef TMPLOPT
	switch(buf->Channels()*100+outchns) {
		case 101:
			dsp_add(dspmeth<1,1>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
			break;
		case 102:
			dsp_add(dspmeth<1,2>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
			break;
		case 201:
			dsp_add(dspmeth<2,1>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
			break;
		case 202:
			dsp_add(dspmeth<2,2>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
			break;
		case 401:
		case 402:
		case 403:
			dsp_add(dspmeth<4,0>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
			break;
		case 404:
			dsp_add(dspmeth<4,4>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
			break;
		default:
			dsp_add(dspmeth<0,0>, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
	}
#else
	dsp_add(dspmeth, 4,this,sp[0]->s_n,sp[0]->s_vec,sp[1+outchns]->s_vec);
#endif
}


V xgroove_obj::m_help()
{
	post("%s - part of xsample objects",thisName());
	post("(C) Thomas Grill, 2001-2002 - version " VERSION " compiled on " __DATE__ " " __TIME__);
#ifdef MAXMSP
	post("Arguments: %s [buffer] [channels=1]",thisName());
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
	post("\tinterp 0/1: interpolation off/on");
	post("\tmin {unit}: set minimum playing point");
	post("\tmax {unit}: set maximum playing point");
	post("\tpos {unit}: set playing position (obeying the current scale mode)");
	post("\tfloat {unit}: set playing position");
	post("\tbang/start: start playing");
	post("\tstop: stop playing");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tsclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("");
}

V xgroove_obj::m_print()
{
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s',buflen = %.3f",buf->Name(),(F)(buf->Frames()*s2u)); 
	post("samples/unit = %.3f, scale mode = %s",(F)(1./s2u),sclmode_txt[sclmode]); 
	post("loop = %s, interpolation = %s",doloop?"yes":"no",interp?"yes":"no"); 
	post("");
}

#ifdef MAXMSP
V xgroove_obj::m_assist(long msg, long arg, char *s)
{
	switch(msg) {
	case 1: //ASSIST_INLET:
		switch(arg) {
		case 0:
			strcpy(s,"Signal of playing speed");
			break;
		case 1:
			strcpy(s,"Starting point");
			break;
		case 2:
			strcpy(s,"Ending point");
			break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		switch(arg) {
		case 0:
			strcpy(s,"Audio signal played");
			break;
		case 1:
			strcpy(s,"Position currently played");
			break;
		case 2:
			strcpy(s,"Starting point (rounded to sample)");
			break;
		case 3:
			strcpy(s,"Ending point (rounded to sample)");
			break;
		}
		break;
	}
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PD
FLEXT_EXT V xgroove_tilde_setup()
#elif defined(MAXMSP)
V main()
#endif
{
	xgroove_obj_setup();
}
#ifdef __cplusplus
}
#endif


