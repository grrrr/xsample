/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
#include <math.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

#define OBJNAME "xgroove~"
#define extobject groove_object

static t_class *extclass;

//#define DEBUG 

// defaults
#define DEF_LOOPMODE 1
#define DEF_SIGMODE 0
#define DEF_SCLMODE 0

#ifdef PD
#define DEF_UNITS 0
#else
#define DEF_UNITS 2
#endif

class extobject
{
public:
#ifdef MAX
    t_pxobject obj;
#else
	t_object obj;
#endif

	t_symbol *bufname;
	F *buf;
	I bchns;
	I sclmode,unitmode;
	
	BL enable,doplay,interp,doloop;
	I buflen;
	D curpos;  // in samples
	I playmin,playmax,playlen;  // in samples

	I sclmin; // in samples
	F sclmul;
	F s2u; // sample to unit conversion factor

	_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { outlet_float(outmin,playmin*s2u); }
	V outputmax() { outlet_float(outmax,playmax*s2u); }
	
	V set_min(F mn);
	V set_max(F mx);
	
	inline F scale(F smp) const { return (smp-sclmin)*sclmul; }
	V setscale(I mode = -1);
	V setunits(I mode = -1);
	V setbuf(t_symbol *s = NULL);

#ifdef PD   
    F defsig;
#endif
};

V extobject::setscale(I mode)
{
	if(mode >= 0) sclmode = mode;
	switch(sclmode) {
		case 0: // samples/units
			sclmin = 0; sclmul = s2u;
			break;
		case 1: // samples/units from recmin to recmax
			sclmin = playmin; sclmul = s2u;
			break;
		case 2: // unity between 0 and buffer size
			sclmin = 0; sclmul = buflen?1.f/buflen:0;
			break;
		case 3:	// unity between recmin and recmax
			sclmin = playmin; sclmul = playlen?1.f/playlen:0;
			break;
		default:
			post("Unknown scale mode");
	}
}

V extobject::setunits(I mode)
{
	if(mode >= 0) unitmode = mode;
	switch(unitmode) {
		case 0: // samples
			s2u = 1;
			break;
		case 1: // buffer size
			s2u = 1.f/buflen;
			break;
		case 2: // ms
			s2u = 1000.f/sys_getsr();
			break;
		case 3: // s
			s2u = 1.f/sys_getsr();
			break;
		default:
			post("Unknown unit mode");
	}
	setscale();
	outputmin();
	outputmax();
}

V extobject::set_min(F mn)
{
	mn /= s2u;  // conversion to samples
	if(mn < 0) mn = 0;
	else if(mn > playmax) mn = playmax;
	playmin = (I)(mn+.5);
	playlen = playmax-playmin;
	setscale();

	outputmin();
}

V extobject::set_max(F mx)
{
	mx /= s2u;  // conversion to samples
	if(mx > buflen) mx = buflen;
	else if(mx < playmin) mx = playmin;
	playmax = (I)(mx+.5);
	playlen = playmax-playmin;
	setscale();
	
	outputmax();
}

static V method_sclmode(extobject *x, t_flint scl)
{
	x->setscale((I)scl);
}

static V method_units(extobject *x, t_flint cnv)
{
	x->setunits((I)cnv);
}


static V method_min(extobject *x,t_float mn)
{
	x->set_min(mn);
}

static V method_max(extobject *x,t_float mx)
{
	x->set_max(mx);
}


static V method_pos(extobject *x,t_float pos)
{
	x->curpos = pos/x->s2u;
	if(x->curpos < x->playmin) x->curpos = x->playmin;
	else if(x->curpos > x->playmax) x->curpos = x->playmax;
}

static V method_loop(extobject *x, t_flint mode)
{
	x->doloop = mode != 0;	
}

static V method_interp(extobject *x, t_flint in)
{
	x->interp = in != 0;	
}

static V method_reset(extobject *x) 
{
	x->curpos = 0;
	x->set_min(0);
    x->set_max(x->buflen*x->s2u);
}

static V method_enable(extobject *x,t_flint en)
{
	x->enable = en != 0;
}

static V method_start(extobject *x)
{ 
	x->doplay = true;
}

static V method_stop(extobject *x)
{
	x->doplay = false;
}

V extobject::setbuf(t_symbol *s)
{
	if(s && *s->s_name)	bufname = s;
	
	const I bufl1 = buflen;

	buf = NULL; 
	bchns = 0;
	buflen = 0; 

	if(bufname) {
#ifdef PD
		t_garray *a;
    
		if (!(a = (t_garray *)pd_findbyclass(bufname, garray_class)))
		{
    		if (*bufname->s_name)
    			error(OBJNAME ": no such array '%s'", bufname->s_name);
    		bufname = 0;
		}
		else if (!garray_getfloatarray(a, &buflen, &buf))
		{
    		error(OBJNAME ": bad template '%s'", bufname->s_name);
    		buf = 0;
			buflen = 0;
		}
		else {
			garray_usedindsp(a);
			bchns = 1;
		}
#elif defined(MAX)
		if(bufname->s_thing) {
			const _buffer *p = (const _buffer *)bufname->s_thing;
			
			if(NOGOOD(p)) {
				post(OBJNAME ": buffer object '%s' no good",bufname->s_name);
			}
			else {
#ifdef DEBUG
				post(OBJNAME ": buffer object '%s' - ???:%i samples:%i channels:%i buflen:%i",bufname->s_name,p->b_valid,p->b_frames,p->b_nchans,p->b_size);
#endif
				buf = p->b_samples;
				bchns = p->b_nchans;
				buflen = p->b_size;
			}
		}
		else {
    		error(OBJNAME ": symbol '%s' not defined", bufname->s_name);
		}
#endif
	}

	if(bufl1 != buflen) method_reset(this); // calls recmin,recmax,rescale
    setunits();
}

static V method_set(extobject *x,t_symbol *s, I argc, t_atom *argv)
{
	x->setbuf(argc >= 1?atom_getsymbolarg(0,argc,argv):NULL);
}



static V *method_new(t_symbol *s, I argc, t_atom *argv)
{
	if(argc < 1) {
		post("Arg error: " OBJNAME " [buffer] [units = %i] [sclmode = %i] [loop = 0] [interp = 1]",(I)DEF_UNITS,(I)DEF_SCLMODE);
		return NULL;
	} 

#ifdef PD	
    extobject *x = (extobject *)pd_new(extclass);

    inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft1"));  // min play pos
    inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft2"));  // max play pos
	outlet_new(&x->obj, &s_signal); // output
	outlet_new(&x->obj, &s_signal); // position
	x->outmin = outlet_new(&x->obj, &s_float); // play min
	x->outmax = outlet_new(&x->obj, &s_float); // play max

    x->defsig = 0;
#elif defined(MAX)
	extobject *x = (extobject *)newobject(extclass);

	// set up inlets and outlets in reverse
	floatin(&x->obj,2);  // max play pos
	floatin(&x->obj,1);  // min play pos
	dsp_setup(&x->obj,1); // speed sig

	x->outmax = outlet_new(&x->obj,"float"); // play max
	x->outmin = outlet_new(&x->obj,"float"); // play min
	outlet_new(&x->obj,"signal"); // position signal
	outlet_new(&x->obj,"signal"); // output signal
#endif

	x->playmin = 0;
	x->playmax = BIGLONG;

	method_sclmode(x,argc >= 3?atom_getflintarg(2,argc,argv):DEF_SCLMODE);
	method_units(x,argc >= 2?atom_getflintarg(1,argc,argv):DEF_UNITS);
	method_loop(x,argc >= 4?atom_getflintarg(3,argc,argv):0);
	method_interp(x,argc >= 5?atom_getflintarg(4,argc,argv):1);

	x->bufname = atom_getsymbolarg(0,argc,argv);
#ifdef PD  // in max it is called by loadbang
	x->setbuf(x->bufname);  // calls reset
#endif

	x->enable = true;
	x->doplay = false;
	
    return x;
}

static V method_free(extobject *x)
{
}



static t_int *method_perform(t_int *w)
{
    extobject *const x = (extobject *)(w[1]);

	if(x->enable) {    
	    I n = (int)(w[5]);    
	    const t_float *speed = (const t_float *)(w[2]);
	    t_float *sig = (t_float *)(w[3]);
	    t_float *pos = (t_float *)(w[4]);

		const I smin = x->playmin,smax = x->playmax;
		const I playlen = x->playlen;
		D o = x->curpos;

		if(x->buf && x->doplay && playlen > 0) {
			const I maxo = smax-3;

			if(x->interp && playlen >= 4) {
				for(I i = 0; i < n; ++i) {	
					const F spd = *(speed++);

					// Offset normalisieren
					if(o >= smax) 
						o = x->doloop?fmod(o-smin,playlen)+smin:(D)smax-0.001;
					else if(o < smin) 
						o = x->doloop?fmod(o+(playlen-smin),playlen)+smin:smin;

					*(pos++) = x->scale(o);

					const I oint = o;
					F a,b,c,d;

					const F *fp = x->buf+oint;
					F frac = o-oint;

					if (oint <= smin) {
						if(x->doloop) {
							// oint ist immer == smin
							a = x->buf[smax-1];
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
								a = b = c = d = x->buf[smin];
							}
						}
					}
					else if(oint > maxo) {
						if(x->doloop) {
							a = fp[-1];
							b = fp[0];
							if(oint == maxo+1) {
								c = fp[1];
								d = x->buf[smin];
							}
							else {
								c = x->buf[smin];
								d = x->buf[smin+1];
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
								a = b = c = d = x->buf[smax-1];
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
				if(x->doloop) {
					for(I i = 0; i < n; ++i) {	
						F spd = *(speed++);

						// Offset normalisieren
						if(o >= smax) 
							o = fmod(o,playlen)+smin;
						else if(o < smin) 
							o = fmod(o+smax,playlen)+smin;

						*(pos++) = x->scale(o);
						*(sig++) = x->buf[(I)o];

						o += spd;
					}
				}
				else {
					for(I i = 0; i < n; ++i) {	
						const F spd = *(speed++);

						*(pos++) = x->scale(o);

						const I oint = o;
						if(oint < smin) 
							*(sig++) = x->buf[smin];
						else if(oint >= smax) 
							*(sig++) = x->buf[smax];
						else
							*(sig++) = x->buf[oint];

						o += spd;
					}
				}
			}
		} 
		else {
			// interne Funktionen benutzen!
			const F oscl = x->scale(o);
			while(n--) {	
				*(pos++) = oscl;
				*(sig++) = 0;
			}
		}
		
		x->curpos = o;
	}   

    return (w+6);
}

static V method_dsp(extobject *x, t_signal **sp)
{
	x->setbuf();
//	x->setunits();  // method_dsp hopefully called at change of sample rate ?!
	dsp_add(method_perform, 5, x, sp[0]->s_vec,sp[1]->s_vec,sp[2]->s_vec,sp[0]->s_n);
}


static V method_help(extobject *x)
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001 - version " VERSION " compiled on " __DATE__ " " __TIME__);
	post("Arguments: " OBJNAME " [buffer] [units = %i] [sclmode = %i] [loop = 0] [interp = 1]",(I)DEF_UNITS,(I)DEF_SCLMODE);
	post("Inlets: 1:Messages/Speed signal, 2:Min position, 3: Max position");
	post("Outlets: 1:Audio signal, 2:Position signal");	
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

static V method_print(extobject *x)
{
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post(OBJNAME " - current settings:");
	post("bufname = '%s',buflen = %.3f",x->bufname?x->bufname->s_name:"",(F)(x->buflen*x->s2u)); 
	post("samples/unit = %.3f, scale mode = %s",(F)(1./x->s2u),sclmode_txt[x->sclmode]); 
	post("loop = %s, interpolation = %s",x->doloop?"yes":"no",x->interp?"yes":"no"); 
	post("");
}

#ifdef MAX
static V method_loadbang(extobject *x)
{
	x->setbuf();
}

static V method_assist(extobject *x, void *b, long msg, long arg, char *s)
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
		}
		break;
	}
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PD
PD_EXTERN 
V xgroove_tilde_setup()
#elif defined(MAX)
V main()
#endif
{
#ifdef PD
	extclass = class_new(gensym(OBJNAME),
    	(t_newmethod)method_new, (t_method)method_free,
    	sizeof(extobject), 0, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(extclass, extobject, defsig);
#elif defined(MAX)
	setup((t_messlist **)&extclass, (method)method_new, (method)method_free, (short)sizeof(extobject), 0L, A_GIMME, A_NULL);
	dsp_initclass();

	addmess((method)method_loadbang, "loadbang", A_CANT, A_NULL);
	addmess((method)method_assist, "assist", A_CANT, A_NULL);
#endif

	add_dsp(extclass,method_dsp);
	
	add_float1(extclass,method_min);
	add_float2(extclass,method_max);

	add_methodG(extclass,method_set, "set");

	add_method1(extclass,method_enable, "enable",A_FLINT);	
	add_method0(extclass,method_help, "help");	
	add_method0(extclass,method_print, "print");	
		
	add_method0(extclass,method_reset, "reset");	
	add_method1(extclass,method_loop, "loop", A_FLINT);	
	add_method1(extclass,method_interp, "interp", A_FLINT);	
	add_method1(extclass,method_min, "min", A_FLOAT);	
	add_method1(extclass,method_max, "max", A_FLOAT);	

	add_bang(extclass,method_start);	
	add_method0(extclass,method_start, "start");	
	add_method0(extclass,method_stop, "stop");	
	add_float(extclass,method_pos);	
	add_method1(extclass,method_pos, "pos", A_FLOAT);	

	add_method1(extclass,method_units, "units", A_FLINT);	
	add_method1(extclass,method_sclmode, "sclmode", A_FLINT);	
}
#ifdef __cplusplus
}
#endif
