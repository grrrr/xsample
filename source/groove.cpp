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

#define OBJNAME "xgroove~"


//#define DEBUG 

class xgroove_obj:
	public xs_obj
{
	CPPEXTERN_HEADER(xgroove_obj,xs_obj)

public:
	xgroove_obj(I argc,t_atom *argv);

#ifdef MAX
	virtual V m_loadbang() { setbuf(); }
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();
	
	virtual V m_start() { doplay = true; }
	virtual V m_stop() { doplay = false; }

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

	_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { outlet_float(outmin,curmin*s2u); }
	V outputmax() { outlet_float(outmax,curmax*s2u); }
	
	V signal(I n,const F *speed,F *sig,F *pos);  // this is the dsp method
	V setbuf(t_symbol *s = NULL);

private:
	virtual V m_dsp(t_signal **sp);

	static V cb_start(t_class *c) { thisClass(c)->m_start(); }
	static V cb_stop(t_class *c) { thisClass(c)->m_stop(); }

	static t_int *dspmeth(t_int *w) 
	{ 
		((xgroove_obj *)w[1])->signal((I)w[2],(const F *)w[3],(F *)w[4],(F *)w[5]); 
		return w+6;
	}

	static V cb_pos(V *c,F pos) { thisClass(c)->m_pos(pos); }
	static V cb_min(V *c,F mn) { thisClass(c)->m_min(mn); }
	static V cb_max(V *c,F mx) { thisClass(c)->m_max(mx); }

	static V cb_reset(V *c) { thisClass(c)->m_reset(); }
	static V cb_loop(V *c,FI lp) { thisClass(c)->m_loop(lp != 0); }
};

CPPEXTERN_NEW_WITH_GIMME(OBJNAME,xgroove_obj)

V xgroove_obj::cb_setup(t_class *c)
{
	add_float1(c,cb_min);
	add_float2(c,cb_max);

	add_method0(c,cb_reset, "reset");	
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
	curpos(0)
{
#ifdef DEBUG
	if(argc < 1) {
		post(OBJNAME " - Warning: no buffer defined");
	} 
#endif
	
#ifdef MAX
	outchns = argc >= 2?atom_getflintarg(1,argc,argv):1;
#else
	outchns = 1;
#endif

#ifdef PD	
    inlet_new(x_obj, &x_obj->ob_pd, &s_float, gensym("ft1"));  // min play pos
    inlet_new(x_obj, &x_obj->ob_pd, &s_float, gensym("ft2"));  // max play pos
    
	int ci;
	for(ci = 0; ci < outchns; ++ci) newout_signal(x_obj); // output
	newout_signal(x_obj); // position
	outmin = newout_float(x_obj); // play min
	outmax = newout_float(x_obj); // play max
#elif defined(MAX)
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

	bufname = argc >= 1?atom_getsymbolarg(0,argc,argv):NULL;
#ifdef PD  // in max it is called by loadbang
	setbuf(bufname);  // calls reset
#endif
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
	outputmin();
}

V xgroove_obj::m_max(F mx)
{
	xs_obj::m_max(mx);
	outputmax();
}


V xgroove_obj::m_pos(F pos)
{
	curpos = pos/s2u;
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}

V xgroove_obj::m_reset() 
{
	xs_obj::setbuf();
	curpos = 0;
	m_min(0);
    m_max(buflen*s2u);
}


V xgroove_obj::setbuf(t_symbol *s)
{
	const I bufl1 = buflen;
	xs_obj::setbuf(s);
	if(bufl1 != buflen) m_reset(); // calls recmin,recmax,rescale
    m_units();
}



V xgroove_obj::signal(I n,const F *speed,F *sig,F *pos)
{
	if(enable) {    
		const I smin = curmin,smax = curmax;
		const I plen = curlen;
		D o = curpos;

		if(buf && doplay && plen > 0) {
			const I maxo = smax-3;

			if(interp && plen >= 4) {
				for(I i = 0; i < n; ++i) {	
					const F spd = *(speed++);

					// Offset normalisieren
					if(o >= smax) 
						o = doloop?fmod(o-smin,plen)+smin:(D)smax-0.001;
					else if(o < smin) 
						o = doloop?fmod(o+(plen-smin),plen)+smin:smin;

					*(pos++) = scale(o);

					const I oint = o;
					F a,b,c,d;

					const F *fp = buf+oint;
					F frac = o-oint;

					if (oint <= smin) {
						if(doloop) {
							// oint ist immer == smin
							a = buf[smax-1];
							b = fp[0];
							c = fp[1];
							d = fp[2];
						}
						else {
							// Offset ist nicht normalisiert
							if(oint == smin) {
								a = b = fp[0];
								c = fp[1];
								d = fp[2];
							}
							else {
								a = b = c = d = buf[smin];
							}
						}
					}
					else if(oint > maxo) {
						if(doloop) {
							a = fp[-1];
							b = fp[0];
							if(oint == maxo+1) {
								c = fp[1];
								d = buf[smin];
							}
							else {
								c = buf[smin];
								d = buf[smin+1];
							}
						}
						else {
							// Offset ist nicht normalisiert
							if(oint == maxo+1) {
								a = fp[-1];
								b = fp[0];
								c = d = fp[1];	
							}
							else 
							if(oint == maxo+2) {
								a = fp[-1];
								b = c = d = fp[0];
							}
							else { 
								// >= (maxo+3 = smax-1)
								a = b = c = d = buf[smax-1];
							}

						}
					}
					else {
						a = fp[-1];
						b = fp[0];
						c = fp[1];
						d = fp[2];
					}

					const F cmb = c-b;
					*(sig++) = b + frac*( 
						cmb - 0.5f*(frac-1.) * ((a-d+3.0f*cmb)*frac + (b-a-cmb))
					);

					o += spd;
				}
			}
			else {
				if(doloop) {
					for(I i = 0; i < n; ++i) {	
						F spd = *(speed++);

						// Offset normalisieren
						if(o >= smax) 
							o = fmod(o,plen)+smin;
						else if(o < smin) 
							o = fmod(o+smax,plen)+smin;

						*(pos++) = scale(o);
						*(sig++) = buf[(I)o];

						o += spd;
					}
				}
				else {
					for(I i = 0; i < n; ++i) {	
						const F spd = *(speed++);

						*(pos++) = scale(o);

						const I oint = o;
						if(oint < smin) 
							*(sig++) = buf[smin];
						else if(oint >= smax) 
							*(sig++) = buf[smax];
						else
							*(sig++) = buf[oint];

						o += spd;
					}
				}
			}
		} 
		else {
			// interne Funktionen benutzen!
			const F oscl = scale(o);
			while(n--) {	
				*(pos++) = oscl;
				*(sig++) = 0;
			}
		}
		
		curpos = o;
	}   
}

V xgroove_obj::m_dsp(t_signal **sp)
{
	setbuf();  // method_dsp hopefully called at change of sample rate ?!
	dsp_add(dspmeth, 5,this,sp[0]->s_n,sp[0]->s_vec,sp[1]->s_vec,sp[2]->s_vec);
}


V xgroove_obj::m_help()
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001-2002 - version " VERSION " compiled on " __DATE__ " " __TIME__);
#ifdef MAX
	post("Arguments: " OBJNAME " [buffer] [channels]");
#else
	post("Arguments: " OBJNAME " [buffer]");
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
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tsclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("");
}

V xgroove_obj::m_print()
{
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post(OBJNAME " - current settings:");
	post("bufname = '%s',buflen = %.3f",bufname?bufname->s_name:"",(F)(buflen*s2u)); 
	post("samples/unit = %.3f, scale mode = %s",(F)(1./s2u),sclmode_txt[sclmode]); 
	post("loop = %s, interpolation = %s",doloop?"yes":"no",interp?"yes":"no"); 
	post("");
}

#ifdef MAX
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
EXT_EXTERN V xgroove_tilde_setup()
#elif defined(MAX)
V main()
#endif
{
	xgroove_obj_setup();
}
#ifdef __cplusplus
}
#endif

