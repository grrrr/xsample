/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
//#include <math.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

#define OBJNAME "xplay~"
#define extobject play_object

static t_class *extclass;

//#define DEBUG 

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
	
	BL enable,doplay;
	I buflen;
	BL interp;

	F s2u; // sample to unit conversion factor

	V setunits(I mode = -1);
	V setbuf(t_symbol *buf = NULL);

#ifdef PD   
    F defsig;
#endif
};

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
}

static V method_units(extobject *x, t_flint cnv)
{
	x->setunits((I)cnv);
}

static V method_interp(extobject *x,t_flint interp)
{
	x->interp = interp != 0;
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
				bchns = p->b_chans;
				buflen = p->b_size;
			}
		}
		else {
    		error(OBJNAME ": symbol '%s' not defined", bufname->s_name);
		}
#endif
	}

    setunits();
}

static V method_set(extobject *x,t_symbol *s, I argc, t_atom *argv)
{
	x->setbuf(argc >= 1?atom_getsymbolarg(0,argc,argv):NULL);
}



static V *method_new(t_symbol *s, I argc, t_atom *argv)
{
	if(argc < 1) {
		post("Arg error: " OBJNAME " [buffer] [units = %i] [interp = 1]",DEF_UNITS);
		return NULL;
	} 
	
#ifdef PD
    extobject *x = (extobject *)pd_new(extclass);

	outlet_new(&x->obj, &s_signal); // output

    x->defsig = 0;

#elif defined(MAX)
	extobject *x = (extobject *)newobject(extclass);

	dsp_setup(&x->obj,1); // pos signal in
	outlet_new(&x->obj,"signal"); // play output
#endif

	method_units(x,argc >= 2?atom_getflintarg(1,argc,argv):DEF_UNITS);
	method_interp(x,argc >= 3?atom_getflintarg(2,argc,argv) != 0:true);

	x->bufname = atom_getsymbolarg(0,argc,argv);
#ifdef PD
	x->setbuf(x->bufname);
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
	    I n = (int)(w[4]);    
	    const t_float *pos = (const t_float *)(w[2]);
	    t_float *sig = (t_float *)(w[3]);

		if(x->doplay && x->buflen > 0) {
			if(x->interp && x->buflen > 4) {
				I maxo = x->buflen-3;

				for(I i = 0; i < n; ++i) {	
					F o = *(pos++)/x->s2u;
					I oint = o;
					F a,b,c,d;

					const F *fp = x->buf+oint;
					F frac = o-oint;

					if (oint < 1) {
						if(oint < 0) {
							frac = 0;
							a = b = c = d = x->buf[0];
						}
						else {
							a = b = fp[0];
							c = fp[1];
							d = fp[2];
						}
					}
					else if(oint > maxo) {
						if(oint == maxo+1) {
							a = fp[-1];	
							b = fp[0];	
							c = d = fp[1];	
						}
						else {
							frac = 0; 
							a = b = c = d = x->buf[x->buflen-1]; 
						}
					}
					else {
						a = fp[-1];
						b = fp[0];
						c = fp[1];
						d = fp[2];
					}

					F cmb = c-b;
					*(sig++) = b + frac*( 
						cmb - 0.5f*(frac-1.) * ((a-d+3.0f*cmb)*frac + (b-a-cmb))
					);
				}
			}
			else {
				// keine Interpolation
				for(I i = 0; i < n; ++i) {	
					I o = *(pos++)/x->s2u;
					if(o < 0) *(sig++) = x->buf[0];
					if(o > x->buflen) 
						*(sig++) = x->buf[x->buflen-1];
					else 
						*(sig++) = x->buf[o];
				}
			}	
		}
		else {
			while(n--) *(sig++) = 0;
		}
	}

    return (w+5);
}

static V method_dsp(extobject *x, t_signal **sp)
{
	x->setunits();  // method_dsp hopefully called at change of sample rate ?!
	dsp_add(method_perform, 4, x, sp[0]->s_vec,sp[1]->s_vec,sp[0]->s_n);
}



static V method_help(extobject *x)
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001 - version " VERSION " compiled on " __DATE__ " " __TIME__);
	post("Arguments: " OBJNAME " [buffer] [units = %i] [interp = 0]",(I)DEF_UNITS);
	post("Inlets: 1:Messages/Position signal");
	post("Outlets: 1:Audio signal");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset name: set buffer");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\tprint: print current settings");
	post("\tbang/start: begin playing");
	post("\tstop: stop playing");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tinterp 0/1: turn interpolation off/on");
	post("");
}

static V method_print(extobject *x)
{
	// print all current settings
	post(OBJNAME " - current settings:");
	post("bufname = '%s',buflen = %.3f",x->bufname?x->bufname->s_name:"",(F)(x->buflen*x->s2u)); 
	post("samples/unit = %.3f,interpolation = %s",(F)(1./x->s2u),x->interp?"yes":"no"); 
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
			strcpy(s,"Messages and Signal of playing position");
			break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		switch(arg) {
		case 0:
			strcpy(s,"Audio signal played");
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
V xplay_tilde_setup()
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
	
	addmess((method)method_loadbang, "loadbang", A_CANT, A_NULL);  // set buffer
	addmess((method)method_assist, "assist", A_CANT, A_NULL);
#endif
	add_dsp(extclass,method_dsp);
	
	add_methodG(extclass,method_set, "set");

	add_method1(extclass,method_enable, "enable",A_FLINT);	
	add_method0(extclass,method_help, "help");	
	add_method0(extclass,method_print, "print");	
		
	add_bang(extclass,method_start);	
	add_method0(extclass,method_start, "start");	
	add_method0(extclass,method_stop, "stop");	

	add_method1(extclass,method_units, "units", A_FLINT);	
	add_method1(extclass,method_interp, "interp", A_FLINT);	
}
#ifdef __cplusplus
}
#endif
