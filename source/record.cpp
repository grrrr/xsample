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

#define OBJNAME "xrecord~"
#define extobject record_object

static t_class *extclass;

//#define DEBUG 

// defaults
#define DEF_LOOPMODE 1
#define DEF_APPMODE 1
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
	BL sigmode,appmode;
	I sclmode,unitmode;

	BL enable,dorec,doloop,domix,dirty;
	L buflen,curpos;  // in samples
	L recmin,recmax,reclen; //,recdone;  // in samples

	L sclmin; // in samples
	F sclmul;
	F s2u; // sample to unit conversion factor

	_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() const { outlet_float(outmin,recmin*s2u); }
	V outputmax() const { outlet_float(outmax,recmax*s2u); }
	
	V set_min(F mn);
	V set_max(F mx);
	
	inline F scale(I smp) const { return (smp-sclmin)*sclmul; }
	V setscale(I mode = -1);
	V setunits(I mode = -1);
	
	V setbuf(t_symbol *bufname = NULL);
	V setdirty();
	V testdirty();

#ifdef PD   
    t_clock *clock;
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
			sclmin = recmin; sclmul = s2u;
			break;
		case 2: // unity between 0 and buffer size
			sclmin = 0; sclmul = buflen?1.f/buflen:0;
			break;
		case 3:	// unity between recmin and recmax
			sclmin = recmin; sclmul = reclen?1.f/reclen:0;
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
#ifdef DEBUG
	post("recmin/recmax = %li/%li, min = %f",recmin,recmax,mn);
#endif
	mn /= s2u;  // conversion to samples
	if(mn < 0) mn = 0;
	else if(mn > recmax) mn = recmax;
	recmin = (L)(mn+.5);
	reclen = recmax-recmin;
	setscale();
#ifdef DEBUG
	post("min = %f/%li",mn,recmin);
#endif
	outputmin();
}

V extobject::set_max(F mx)
{
#ifdef DEBUG
	post("recmin/recmax = %li/%li, max = %f",recmin,recmax,mx);
#endif
	mx /= s2u;  // conversion to samples
	if(mx > buflen) mx = buflen;
	else if(mx < recmin) mx = recmin;
	recmax = (L)(mx+.5);
	reclen = recmax-recmin;
	setscale();
#ifdef DEBUG
	post("max = %f/%li",mx,recmax);
#endif
	outputmax();
}


static V method_mixmode(extobject *x, t_flint mx)
{
	x->domix = mx != 0;
}

static V method_sigmode(extobject *x, t_flint mode)
{
	x->sigmode = mode != 0;	
	x->dorec = x->sigmode;
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
	x->curpos = (L)(pos/x->s2u+.5);
	if(x->curpos < x->recmin) x->curpos = x->recmin;
	else if(x->curpos > x->recmax) x->curpos = x->recmax;
}

static V method_loop(extobject *x, t_flint mode)
{
	x->doloop = mode != 0;	
}

static V method_append(extobject *x, t_flint mode)
{
	x->appmode = mode != 0;	
	if(!x->appmode) method_pos(x,0);
}


static V method_start(extobject *x)
{
	x->dorec = true;
}

static V method_stop(extobject *x)
{
	x->dorec = false;
	if(!x->sigmode && !x->appmode) method_pos(x,0);
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
				post(OBJNAME ": buffer object '%s' - samples:%i channels:%i buflen:%i",bufname->s_name,p->b_frames,p->b_nchans,p->b_size);
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

	method_reset(this); // calls recmin,recmax,rescale
    setunits();
}

	
V extobject::setdirty()
{
	if(bufname) {
#ifdef PD
		t_garray *a;
    
		if (!(a = (t_garray *)pd_findbyclass(bufname, garray_class))) {
    		if (*bufname->s_name)
    			error(OBJNAME ": no such array '%s'", bufname->s_name);
		}
		else if (!garray_getfloatarray(a, &buflen, &buf)) {
    		error(OBJNAME ": bad template '%s'", bufname->s_name);
		}
		else garray_redraw(a);
#elif defined(MAX)
		if(bufname->s_thing) {
			_buffer *p = (_buffer *)bufname->s_thing;
			
			if(NOGOOD(p)) {
				post(OBJNAME ": buffer object '%s' no good",bufname->s_name);
			}
			else {
				p->b_modtime = gettime();
			}
		}
		else {
    		error(OBJNAME ": symbol '%s' not defined", bufname->s_name);
		}
#endif 
	}
}
	
/*
V extobject::testdirty()
{
	if(bufname) {
#ifdef PD
		t_garray *a = (t_garray *)pd_findbyclass(bufname, garray_class);
    	if(a) garray_redraw(a);
#elif defined(MAX)
		if(bufname->s_thing) {
			_buffer *p = (_buffer *)bufname->s_thing;
			post("s_thing: valid:%i chns:%i buflen:%i",p->b_valid,p->b_nchans,p->b_size);
			post("vol:%i space:%i modtime:%li time:%li",(I)p->b_vol,(I)p->b_space,p->b_modtime,(L)gettime());
			post("selection:%i..%i",p->b_selbegin[0],p->b_selend[0]);
		}
		else {
    		error(OBJNAME ": symbol '%s' not defined", bufname->s_name);
		}
#endif
	}
}
*/


static V method_set(extobject *x,t_symbol *s, I argc, t_atom *argv)
{
	x->setbuf(argc >= 1?atom_getsymbolarg(0,argc,argv):NULL);
}

#ifdef PD
static V method_tick(extobject *x)
{
	if(x->dirty) {
	    t_garray *a = (t_garray *)pd_findbyclass(x->bufname, garray_class);
		if (a) bug("tabwrite_tilde_tick");
		else garray_redraw(a);

		x->dirty = false;
	}
}
#endif

static V *method_new(t_symbol *s, I argc, t_atom *argv)
{
	if(argc < 1) {
		post("Arg error: " OBJNAME " [buffer] [units = %i] [sclmode = %i] [sigmode = %i] [append = 1] [loop = 0] [mixmode = 0]",(I)DEF_UNITS,(I)DEF_SCLMODE,(I)DEF_SIGMODE);
		return NULL;
	} 
	
#ifdef PD
    extobject *x = (extobject *)pd_new(extclass);

    inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);  
    inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft2"));  
    inlet_new(&x->obj, &x->obj.ob_pd, &s_float, gensym("ft3"));  

	outlet_new(&x->obj, &s_signal);
	x->outmin = outlet_new(&x->obj, &s_float);
	x->outmax = outlet_new(&x->obj, &s_float);

    x->clock = clock_new(x,(t_method)method_tick);
	x->dirty = false;
    x->defsig = 0;
#elif defined(MAX)
	extobject *x = (extobject *)newobject(extclass);

	// inlets and outlets set up in reverse
	floatin(&x->obj,3);  // max record pos
	floatin(&x->obj,2);  // min record pos
	dsp_setup(&x->obj,2); // sound and on/off signal in

	x->outmax = outlet_new(&x->obj,"float");  // max record out
	x->outmin = outlet_new(&x->obj,"float");  // min record out
	outlet_new(&x->obj,"signal");  // pos signal out
#endif

	x->recmin = 0;
	x->recmax = BIGLONG;

	method_sclmode(x,argc >= 3?atom_getflintarg(2,argc,argv):DEF_SCLMODE);
	method_units(x,argc >= 2?atom_getflintarg(1,argc,argv):DEF_UNITS);
	method_sigmode(x,argc >= 4?atom_getflintarg(3,argc,argv):0);
	method_append(x,argc >= 5?atom_getflintarg(4,argc,argv):1);
	method_loop(x,argc >= 6?atom_getflintarg(5,argc,argv):0);
	method_mixmode(x,argc >= 7?atom_getflintarg(6,argc,argv):0);

	x->bufname = atom_getsymbolarg(0,argc,argv);
#ifdef PD  // in max it is called by loadbang
	x->setbuf(x->bufname);  // calls reset
#endif

	x->dorec = false;
	x->enable = true;
	
    return x;
}

static V method_free(extobject *x)
{
#ifdef PD
	clock_free(x->clock);
#endif
}



static t_int *method_perform(t_int *w)
{
    extobject *const x = (extobject *)(w[1]);
    
	if(x->enable) {
	    I n = (int)(w[5]);    
	    const t_float *sig = (const t_float *)(w[2]);
	    const t_float *on = (const t_float *)(w[3]);
	    t_float *pos = (t_float *)(w[4]);

		register L o = x->curpos;

		if(o < x->recmin) o = x->recmin;

		if(x->buf && x->reclen > 0) {
			while(n) {
				L ncur = x->recmax-o; // maximal bis zum Bufferende bzw. Aufnahmeende

				if(ncur <= 0) {	// Ende des Buffers
					o = x->recmin;
					if(x->doloop) 
						ncur = x->reclen;
					else 
						x->dorec = false;
				}

				if(!x->dorec) break;

				if(ncur > n) ncur = n;
				
				register I i;
				register F *bf = x->buf+o;

				if(x->sigmode) {
					if(x->appmode) {
						// append to current position
					
						if(!x->domix) {
							for(i = 0; i < ncur; ++i) {	
								if(*(on++) >= 0) {
									x->buf[o] = *(sig++);						
									*(pos++) = x->scale(o++);
								}
								else
									*(pos++) = x->scale(o);
							}
						}
						else {
							for(i = 0; i < ncur; ++i) {	
								register F g = *(on++);
								if(g >= 0) {
									x->buf[o] = x->buf[o]*(1.-g)+*(sig++)*g;
									*(pos++) = x->scale(o++);
								}
								else 
									*(pos++) = x->scale(o);
							}
						}
					}
					else {  
						// don't append
						if(!x->domix) {
							for(i = 0; i < ncur; ++i) {	
								if(*(on++) >= 0)
								{ 	
									x->buf[o] = *(sig++);
									*(pos++) = x->scale(o++);
								}
								else 
									*(pos++) = x->scale(o = 0);
							}
						}
						else {
							for(i = 0; i < ncur; ++i) {	
								register F g = *(on++);
								if(g >= 0) {
									x->buf[o] = x->buf[o]*(1.-g)+*(sig++)*g;
									*(pos++) = x->scale(o++);
								}
								else
									*(pos++) = x->scale(o = 0);
							}
						}
					}
				}
				else { 
					// message mode
					
					// Altivec optimization for that!
					if(!x->domix) {
						for(i = 0; i < ncur; ++i) {	
							x->buf[o] = *(sig++);
							*(pos++) = x->scale(o++);
						}
					}
					else {
						for(i = 0; i < ncur; ++i) {	
							x->buf[o] = *(sig++)* *(on++);
							*(pos++) = x->scale(o++);
						}
					}
				}

				n -= ncur;
				x->dirty = true;
	#ifdef PD
	    	    clock_delay(x->clock, 10);
	#endif
			} 
			x->curpos = o;
			
	#ifdef MAX
			if(x->dirty) { x->setdirty(); x->dirty = false; }
	#endif
		}

		while(n--) *(pos++) = x->scale(o);
	}

    return (w+6);
}

static V method_dsp(extobject *x, t_signal **sp)
{
	x->setunits();  // method_dsp hopefully called at change of sample rate ?!
	dsp_add(method_perform, 5, x, sp[0]->s_vec,sp[1]->s_vec,sp[2]->s_vec,sp[0]->s_n);
}




static V method_help(extobject *x)
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001 - version " VERSION " compiled on " __DATE__ " " __TIME__);
	post("Arguments: " OBJNAME " [buffer] [units = %i] [sclmode = %i] [sigmode = %i] [append = 1] [loop = 0] [mixmode = 0]",(I)DEF_UNITS,(I)DEF_SCLMODE,(I)DEF_SIGMODE);
	post("Inlets: 1:Messages/Audio signal, 2:Trigger signal, 3:Min point, 4: Max point");
	post("Outlets: 1:Position signal, 2:Min point, 3:Max point");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset [name]: set buffer or reinit");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\treset: reset min/max recording points and recording offset");
	post("\tprint: print current settings");
	post("\tsigmode 0/1: specify message or signal triggered recording");
	post("\tappend 0/1: reset recording position or append to current position");
	post("\tloop 0/1: switches looping off/on");
	post("\tmixmode 0/1: specify if audio signal should be mixed in");
	post("\tmin {unit}: set minimum recording point");
	post("\tmax {unit}: set maximum recording point");
	post("\tpos {unit}: set recording position (obeying the current scale mode)");
	post("\tfloat {unit}: set recording position");
	post("\tbang/start: start recording");
	post("\tstop: stop recording");
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
	post("sigmode = %s, append = %s, loop = %s, mixmode = %s",x->sigmode?"yes":"no",x->appmode?"yes":"no",x->doloop?"yes":"no",x->domix?"yes":"no"); 
	post("");
}

/*
static V method_test(extobject *x)
{
	x->testdirty();
}
*/

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
			strcpy(s,"Messages and Audio signal to record");
			break;
		case 1:
			strcpy(s,"On/Off/Fade signal (0..1)");
			break;
		case 2:
			strcpy(s,"Starting point of recording");
			break;
		case 3:
			strcpy(s,"Ending point of recording");
			break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		switch(arg) {
		case 0:
			strcpy(s,"Current position of recording");
			break;
		case 1:
			strcpy(s,"Starting point of recording");
			break;
		case 2:
			strcpy(s,"Ending point of recording");
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
V xrecord_tilde_setup()
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
	
	add_float2(extclass,method_min);
	add_float3(extclass,method_max);

	add_methodG(extclass,method_set, "set");

	add_method1(extclass,method_enable, "enable",A_FLINT);	
	add_method0(extclass,method_help, "help");	
	add_method0(extclass,method_print, "print");	
//	add_method0(extclass,method_test, "test");	
		
	add_method0(extclass,method_reset, "reset");	
	add_method1(extclass,method_mixmode, "mixmode", A_FLINT);	
	add_method1(extclass,method_sigmode, "sigmode", A_FLINT);	
	add_method1(extclass,method_loop, "loop", A_FLINT);	
	add_method1(extclass,method_append, "append", A_FLINT);	
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
