/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

#define OBJNAME "xplay~"

//#define DEBUG 


class xplay_obj:
	public xs_obj
{
	CPPEXTERN_HEADER(xplay_obj,xs_obj)

public:
	xplay_obj(I argc, t_atom *argv);
	~xplay_obj();
	
#ifdef MAXMSP
	virtual V m_loadbang() { buf->Set(); }
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();
	
	virtual I m_set(I argc,t_atom *argv);

	virtual V m_start() { m_refresh(); doplay = true; }
	virtual V m_stop() { doplay = false; }

protected:
	BL doplay;
	I outchns;
	F **outvecs;

#ifdef TMPLOPT
	template <int _BCHNS_,int _OCHNS_>
#endif
	V signal(I n,const F *pos);  // this is the dsp method

private:
	virtual V m_dsp(t_signal **sp);

	static V cb_start(t_class *c) { thisClass(c)->m_start(); }
	static V cb_stop(t_class *c) { thisClass(c)->m_stop(); }
	static V cb_reset(t_class *c) { thisClass(c)->m_reset(); }

#ifdef TMPLOPT
	template <int _BCHNS_,int _OCHNS_>
#endif
	static t_int *dspmeth(t_int *w); 
};

CPPEXTERN_NEW_WITH_GIMME(OBJNAME,xplay_obj)


V xplay_obj::cb_setup(t_class *c)
{
	add_bang(c,cb_start);
	add_method0(c,cb_start,"start");
	add_method0(c,cb_stop,"stop");
}


xplay_obj::xplay_obj(I argc, t_atom *argv): 
	doplay(false),outvecs(NULL)
{
#ifdef DEBUG
	if(argc < 1) {
		post(OBJNAME " - Warning: no buffer defined");
	} 
#endif
	
#ifdef MAXMSP
	dsp_setup(x_obj,1); // pos signal in
	outchns = argc >= 2?atom_getflintarg(1,argc,argv):1;
#else
	outchns = 1;
#endif

	int ci;
	for(ci = 0; ci < outchns; ++ci)
		newout_signal(x_obj); // output

	buf = new buffer(argc >= 1?atom_getsymbolarg(0,argc,argv):NULL);	
}

xplay_obj::~xplay_obj()
{
	if(buf) delete buf;
	if(outvecs) delete[] outvecs;
}

I xplay_obj::m_set(I argc,t_atom *argv) 
{
	I r = xs_obj::m_set(argc,argv);
	if(r != 0) m_units();
	return r;
}


#ifdef TMPLOPT
template <int _BCHNS_,int _OCHNS_>
t_int *xplay_obj::dspmeth(t_int *w) 
{ 
	((xplay_obj *)w[1])->signal<_BCHNS_,_OCHNS_>((I)w[2],(const F *)w[3]); 
	return w+4;
}
#else
t_int *xplay_obj::dspmeth(t_int *w) 
{ 
	((xplay_obj *)w[1])->signal((I)w[2],(const F *)w[3]); 
	return w+4;
}
#endif


#ifdef TMPLOPT
template <int _BCHNS_,int _OCHNS_>
#endif
V xplay_obj::signal(I n,const F *pos)
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
	
		if(doplay && buf->Frames() > 0) {
			if(interp == xsi_4p && buf->Frames() > 4) {
				I maxo = buf->Frames()-3;

				for(I i = 0; i < n; ++i,++si) {	
					F o = *(pos++)/s2u;
					I oint = o;
					register F a,b,c,d;

					for(I ci = 0; ci < OCHNS; ++ci) {
						register const F *fp = buf->Data()+oint*BCHNS+ci;
						F frac = o-oint;

						if (oint < 1) {
							if(oint < 0) {
								frac = 0;
								a = b = c = d = buf->Data()[0*BCHNS+ci];
							}
							else {
								a = b = fp[0*BCHNS];
								c = fp[1*BCHNS];
								d = fp[2*BCHNS];
							}
						}
						else if(oint > maxo) {
							if(oint == maxo+1) {
								a = fp[-1*BCHNS];	
								b = fp[0*BCHNS];	
								c = d = fp[1*BCHNS];	
							}
							else {
								frac = 0; 
								a = b = c = d = buf->Data()[(buf->Frames()-1)*BCHNS+ci]; 
							}
						}
						else {
							a = fp[-1*BCHNS];
							b = fp[0*BCHNS];
							c = fp[1*BCHNS];
							d = fp[2*BCHNS];
						}

						F cmb = c-b;
						sig[ci][si] = b + frac*( 
							cmb - 0.5f*(frac-1.) * ((a-d+3.0f*cmb)*frac + (b-a-cmb))
						);
					}
				}
			}
			else {
				// keine Interpolation
				for(I i = 0; i < n; ++i,++si) {	
					I o = *(pos++)/s2u;
					if(o < 0) {
						for(I ci = 0; ci < OCHNS; ++ci)
							sig[ci][si] = buf->Data()[0*BCHNS+ci];
					}
					if(o > buf->Frames()) {
						for(I ci = 0; ci < OCHNS; ++ci)
							sig[ci][si] = buf->Data()[(buf->Frames()-1)*BCHNS+ci];
					}
					else {
						for(I ci = 0; ci < OCHNS; ++ci)
							sig[ci][si] = buf->Data()[o*BCHNS+ci];
					}
				}
			}	
		}
		else {
			while(n--) { 
				for(I ci = 0; ci < OCHNS; ++ci)	sig[ci][si] = 0;
				++si;
			}
		}
	}
}

V xplay_obj::m_dsp(t_signal **sp)
{
	m_refresh();  // m_dsp hopefully called at change of sample rate ?!

	if(outvecs) delete[] outvecs;
	outvecs = new F *[buf->Frames()];
	for(I ci = 0; ci < buf->Frames(); ++ci)
		outvecs[ci] = sp[1+ci%outchns]->s_vec;

#ifdef TMPLOPT
	switch(buf->Frames()*100+outchns) {
		case 101:
			dsp_add(dspmeth<1,1>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		case 102:
			dsp_add(dspmeth<1,2>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		case 201:
			dsp_add(dspmeth<2,1>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		case 202:
			dsp_add(dspmeth<2,2>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		case 204:
			dsp_add(dspmeth<2,4>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		case 402:
			dsp_add(dspmeth<4,2>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		case 404:
			dsp_add(dspmeth<4,4>, 3,this,sp[0]->s_n,sp[0]->s_vec);
			break;
		default:
			dsp_add(dspmeth<0,0>, 3,this,sp[0]->s_n,sp[0]->s_vec);
	}
#else
	dsp_add(dspmeth, 3,this,sp[0]->s_n,sp[0]->s_vec);
#endif
}



V xplay_obj::m_help()
{
	post(OBJNAME " - part of xsample objects");
	post("(C) Thomas Grill, 2001-2002 - version " VERSION " compiled on " __DATE__ " " __TIME__);
#ifdef MAXMSP
	post("Arguments: " OBJNAME " [buffer] [channels=1]");
#else
	post("Arguments: " OBJNAME " [buffer]");
#endif
	post("Inlets: 1:Messages/Position signal");
	post("Outlets: 1:Audio signal");	
	post("Methods:");
	post("\thelp: shows this help");
	post("\tset name: set buffer");
	post("\tenable 0/1: turn dsp calculation off/on");	
	post("\tprint: print current settings");
	post("\tbang/start: begin playing");
	post("\tstop: stop playing");
	post("\treset: recognizes new buffer");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tinterp 0/1: turn interpolation off/on");
	post("");
}

V xplay_obj::m_print()
{
	// print all current settings
	post(OBJNAME " - current settings:");
	post("bufname = '%s',buflen = %.3f",buf->Name(),(F)(buf->Frames()*s2u)); 
	post("samples/unit = %.3f,interpolation = %s",(F)(1./s2u),interp?"yes":"no"); 
	post("");
}


#ifdef MAXMSP
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
#elif defined(MAXMSP)
V main()
#endif
{
	xplay_obj_setup();
}
#ifdef __cplusplus
}
#endif
