/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
//#include <math.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

#define OBJNAME "xrecord~"

class xrec_obj:
	public xs_obj
{
	CPPEXTERN_HEADER(xrec_obj,xs_obj)

public:
	xrec_obj(I argc,t_atom *argv);
	~xrec_obj();
	
#ifdef MAX
	virtual V m_loadbang() { setbuf();	}
	virtual V m_assist(L msg,L arg,C *s);
#endif
	
	virtual V m_help();
	virtual V m_print();
	
	virtual V m_units(xs_unit md = xsu__);
	virtual V m_min(F mn);
	virtual V m_max(F mx);

	virtual V m_mixmode(BL mx) { mixmode = mx; }
	virtual V m_sigmode(BL mode) { dorec = sigmode = mode; }
	virtual V m_loop(BL lp) { doloop = lp; }
	virtual V m_append(BL app) { if(!(appmode = app)) m_pos(0); }
	
	virtual V m_pos(F pos);
	virtual V m_start() { dorec = true; }
	virtual V m_stop() { dorec = false; if(!sigmode && !appmode) m_pos(0); }
	virtual V m_reset();

	virtual V setbuf(t_symbol *s = NULL);

protected:
	I inchns;
	BL sigmode,appmode;
	BL dirty;

	BL dorec,doloop,mixmode;
	L curpos;  // in samples

	_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() const { outlet_float(outmin,curmin*s2u); }
	V outputmax() const { outlet_float(outmax,curmax*s2u); }
	
	V signal(I n,const F *in,const F *on,F *pos);  // this is the dsp method

private:
	virtual V m_dsp(t_signal **sp);

	static V cb_start(t_class *c) { thisClass(c)->m_start(); }
	static V cb_stop(t_class *c) { thisClass(c)->m_stop(); }

	static t_int *dspmeth(t_int *w) 
	{ 
		thisClass(w+1)->signal((I)w[2],(const F *)w[3],(const F *)w[4],(F *)w[5]); 
		return w+6;
	}

	static V cb_pos(V *c,F pos) { thisClass(c)->m_pos(pos); }
	static V cb_min(V *c,F mn) { thisClass(c)->m_min(mn); }
	static V cb_max(V *c,F mx) { thisClass(c)->m_max(mx); }

	static V cb_reset(V *c) { thisClass(c)->m_reset(); }
	static V cb_loop(V *c,FI lp) { thisClass(c)->m_loop(lp != 0); }

	static V cb_mixmode(V *c,FI md) { thisClass(c)->m_mixmode(md != 0); }
	static V cb_sigmode(V *c,FI md) { thisClass(c)->m_sigmode(md != 0); }
	static V cb_append(V *c,FI md) { thisClass(c)->m_append(md != 0); }
};


CPPEXTERN_NEW_WITH_GIMME(OBJNAME,xrec_obj)

V xrec_obj::cb_setup(t_class *c)
{
	add_float2(c,cb_min);
	add_float3(c,cb_max);

	add_method0(c,cb_reset, "reset");	
	add_method1(c,cb_mixmode, "mixmode", A_FLINT);	
	add_method1(c,cb_sigmode, "sigmode", A_FLINT);	
	add_method1(c,cb_loop, "loop", A_FLINT);	
	add_method1(c,cb_append, "append", A_FLINT);	
	add_method1(c,cb_min, "min", A_FLOAT);	
	add_method1(c,cb_max, "max", A_FLOAT);	

	add_bang(c,cb_start);	
	add_method0(c,cb_start, "start");	
	add_method0(c,cb_stop, "stop");	
	add_float(c,cb_pos);	
	add_method1(c,cb_pos, "pos", A_FLOAT);	
}

xrec_obj::xrec_obj(I argc,t_atom *argv):
	dorec(false),
	sigmode(false),mixmode(false),
	appmode(true),doloop(false),
	dirty(false)
{
#ifdef DEBUG
	if(argc < 1) {
		post(OBJNAME " - Warning: no buffer defined");
	} 
#endif

#ifdef MAX
	inchns = argc >= 2?atom_getflintarg(1,argc,argv):1;
#else
	inchns = 1;
#endif

#ifdef PD
	I ci;
	for(ci = 0; ci < inchns; ++ci)
    	inlet_new(x_obj, &x_obj->ob_pd, &s_signal, &s_signal);  // sound in
    	
    inlet_new(x_obj, &x_obj->ob_pd, &s_float, gensym("ft2"));  
    inlet_new(x_obj, &x_obj->ob_pd, &s_float, gensym("ft3"));  

	newout_signal(x_obj);
	outmin = newout_float(x_obj);
	outmax = newout_float(x_obj);

//    clock = clock_new(x,(t_method)method_tick);
#elif defined(MAX)
	// inlets and outlets set up in reverse
	floatin(x_obj,3);  // max record pos
	floatin(x_obj,2);  // min record pos
	dsp_setup(x_obj,inchns+1); // sound and on/off signal in

	outmax = newout_float(x_obj);  // max record out
	outmin = newout_float(x_obj);  // min record out
	newout_signal(x_obj);  // pos signal out
#endif

	bufname = atom_getsymbolarg(0,argc,argv);
#ifdef PD  // in max it is called by loadbang
	setbuf(bufname);  // calls reset
#endif
}

xrec_obj::~xrec_obj()
{
#ifdef PD
//	clock_free(clock);
#endif
}


V xrec_obj::m_units(xs_unit mode)
{
	xs_obj::m_units(mode);

	m_sclmode();
	outputmin();
	outputmax();
}

V xrec_obj::m_min(F mn)
{
	xs_obj::m_min(mn);
	outputmin();
}

V xrec_obj::m_max(F mx)
{
	xs_obj::m_max(mx);
	outputmax();
}


V xrec_obj::m_pos(F pos)
{
	curpos = (L)(pos/s2u+.5);
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}

V xrec_obj::m_reset()
{
	curpos = 0;
	m_min(0);
    m_max(buflen*s2u);
}


V xrec_obj::setbuf(t_symbol *s)
{
	xs_obj::setbuf(s);
	m_reset(); // calls recmin,recmax,rescale
    m_units();
}

	
#ifdef PD
/*
V xrec_obj::m_tick()
{
	if(x->dirty) {
	    t_garray *a = (t_garray *)pd_findbyclass(x->bufname, garray_class);
		if (a) bug("tabwrite_tilde_tick");
		else garray_redraw(a);

		x->dirty = false;
	}
}
*/
#endif



V xrec_obj::signal(I n,const F *sig,const F *on,F *pos)
{
	if(enable) {
		register L o = curpos;

		if(o < curmin) o = curmin;

		if(buf && curlen > 0) {
			while(n) {
				L ncur = curmax-o; // maximal bis zum Bufferende bzw. Aufnahmeende

				if(ncur <= 0) {	// Ende des Buffers
					o = curmin;
					if(doloop) 
						ncur = curlen;
					else 
						dorec = false;
				}

				if(!dorec) break;

				if(ncur > n) ncur = n;
				
				register I i;
				register F *bf = buf+o;

				if(sigmode) {
					if(appmode) {
						// append to current position
					
						if(!mixmode) {
							for(i = 0; i < ncur; ++i) {	
								if(*(on++) >= 0) {
									buf[o] = *(sig++);						
									*(pos++) = scale(o++);
								}
								else
									*(pos++) = scale(o);
							}
						}
						else {
							for(i = 0; i < ncur; ++i) {	
								register F g = *(on++);
								if(g >= 0) {
									buf[o] = buf[o]*(1.-g)+*(sig++)*g;
									*(pos++) = scale(o++);
								}
								else 
									*(pos++) = scale(o);
							}
						}
					}
					else {  
						// don't append
						if(!mixmode) {
							for(i = 0; i < ncur; ++i) {	
								if(*(on++) >= 0)
								{ 	
									buf[o] = *(sig++);
									*(pos++) = scale(o++);
								}
								else 
									*(pos++) = scale(o = 0);
							}
						}
						else {
							for(i = 0; i < ncur; ++i) {	
								register F g = *(on++);
								if(g >= 0) {
									buf[o] = buf[o]*(1.-g)+*(sig++)*g;
									*(pos++) = scale(o++);
								}
								else
									*(pos++) = scale(o = 0);
							}
						}
					}
				}
				else { 
					// message mode
					
					// Altivec optimization for that!
					if(!mixmode) {
						for(i = 0; i < ncur; ++i) {	
							buf[o] = *(sig++);
							*(pos++) = scale(o++);
						}
					}
					else {
						for(i = 0; i < ncur; ++i) {	
							buf[o] = *(sig++)* *(on++);
							*(pos++) = scale(o++);
						}
					}
				}

				n -= ncur;
	#ifdef PD
				dirty = true;
//	    	    clock_delay(clock, 10);
	#endif
			} 
			curpos = o;
			
	#ifdef MAX
			if(dirty) { setdirty(); dirty = false; }
	#endif
		}

		while(n--) *(pos++) = scale(o);
	}
}

V xrec_obj::m_dsp(t_signal **sp)
{
	m_units();  // method_dsp hopefully called at change of sample rate ?!
	dsp_add(dspmeth, 5,x_obj,sp[0]->s_n,sp[0]->s_vec,sp[1]->s_vec,sp[2]->s_vec);
}




V xrec_obj::m_help()
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001-2002 - version " VERSION " compiled on " __DATE__ " " __TIME__);
#ifdef MAX
	post("Arguments: " OBJNAME " [buffer] [channels]");
#else
	post("Arguments: " OBJNAME " [buffer]");
#endif
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

V xrec_obj::m_print()
{
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post(OBJNAME " - current settings:");
	post("bufname = '%s',buflen = %.3f",bufname?bufname->s_name:"",(F)(buflen*s2u)); 
	post("samples/unit = %.3f, scale mode = %s",(F)(1./s2u),sclmode_txt[sclmode]); 
	post("sigmode = %s, append = %s, loop = %s, mixmode = %s",sigmode?"yes":"no",appmode?"yes":"no",doloop?"yes":"no",mixmode?"yes":"no"); 
	post("");
}


#ifdef MAX
V xrec_obj::m_assist(L msg,L arg,C *s)
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
EXT_EXTERN V xrecord_tilde_setup()
#elif defined(MAX)
V main()
#endif
{
	xrec_obj_setup();
}
#ifdef __cplusplus
}
#endif
