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

#define OBJNAME "xplay~"

//#define DEBUG 


class xplay_obj:
	public xp_obj
{
	CPPEXTERN_HEADER(xplay_obj,xp_obj)

public:
	xplay_obj(I argc, t_atom *argv);
	
#ifdef MAX
	virtual V m_loadbang() { setbuf(); }
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();
	
	virtual V m_start() { doplay = true; }
	virtual V m_stop() { doplay = false; }

	virtual V setbuf(t_symbol *s = NULL) 
	{
		xp_obj::setbuf(s);
	    m_units();
	}

protected:
	BL doplay;
	I outchns;

	V signal(I n,const F *pos,F *sig);  // this is the dsp method

private:
	virtual V m_dsp(t_signal **sp);

	static V cb_start(t_class *c) { thisClass(c)->m_start(); }
	static V cb_stop(t_class *c) { thisClass(c)->m_stop(); }

	static t_int *dspmeth(t_int *w) 
	{ 
		thisClass(w+1)->signal((I)w[2],(const F *)w[3],(F *)w[4]); 
		return w+5;
	}
};

CPPEXTERN_NEW_WITH_GIMME(xplay_obj)


V xplay_obj::cb_setup(t_class *c)
{
	add_bang(c,cb_start);
	add_method0(c,cb_start,"start");
	add_method0(c,cb_stop,"stop");
}


xplay_obj::xplay_obj(I argc, t_atom *argv): 
	doplay(false),outchns(0)
{
#ifdef DEBUG
	if(argc < 1) {
		post(OBJNAME " - Warning: no buffer defined");
	} 
#endif
	
#ifdef MAX
	dsp_setup(x_obj,1); // pos signal in
#endif

	outchns = argc >= 2?atom_getflintarg(1,argc,argv):0;
	int ci;
	for(ci = 0; ci < outchns; ++ci)
		newout_signal(x_obj); // output

	bufname = argc >= 1?atom_getsymbolarg(0,argc,argv):NULL;	
#ifdef PD
	setbuf(bufname);
#endif
}


V xplay_obj::signal(I n,const F *pos,F *sig)
{
	if(enable) {
		if(doplay && buflen > 0) {
			if(interp && buflen > 4) {
				I maxo = buflen-3;

				for(I i = 0; i < n; ++i) {	
					F o = *(pos++)/s2u;
					I oint = o;
					F a,b,c,d;

					const F *fp = buf+oint;
					F frac = o-oint;

					if (oint < 1) {
						if(oint < 0) {
							frac = 0;
							a = b = c = d = buf[0];
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
							a = b = c = d = buf[buflen-1]; 
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
					I o = *(pos++)/s2u;
					if(o < 0) *(sig++) = buf[0];
					if(o > buflen) 
						*(sig++) = buf[buflen-1];
					else 
						*(sig++) = buf[o];
				}
			}	
		}
		else {
			while(n--) *(sig++) = 0;
		}
	}
}

V xplay_obj::m_dsp(t_signal **sp)
{
	m_units();  // method_dsp hopefully called at change of sample rate ?!
	dsp_add(dspmeth, 4,x_obj,sp[0]->s_n,sp[0]->s_vec,sp[1]->s_vec);
}



V xplay_obj::m_help()
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001-2002 - version " VERSION " compiled on " __DATE__ " " __TIME__);
	post("Arguments: " OBJNAME " [buffer] [channels]");
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

V xplay_obj::m_print()
{
	// print all current settings
	post(OBJNAME " - current settings:");
	post("bufname = '%s',buflen = %.3f",bufname?bufname->s_name:"",(F)(buflen*s2u)); 
	post("samples/unit = %.3f,interpolation = %s",(F)(1./s2u),interp?"yes":"no"); 
	post("");
}


#ifdef MAX
V xplay_obj::m_assist(L msg,L arg,C *s)
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
EXT_EXTERN V xplay_tilde_setup()
#elif defined(MAX)
V main()
#endif
{
	xplay_obj_setup();
}
#ifdef __cplusplus
}
#endif
