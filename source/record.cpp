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


class xrecord:
	public xsample
{
	FLEXT_HEADER(xrecord,xsample)

public:
	xrecord(I argc,t_atom *argv);
	
#ifdef MAXMSP
	virtual V m_assist(L msg,L arg,C *s);
#endif
	
	virtual V m_help();
	virtual V m_print();
	
	virtual I m_set(I argc,t_atom *argv);

	virtual V m_pos(F pos);
	virtual V m_start();
	virtual V m_stop();

	virtual V m_reset();

	virtual V m_units(xs_unit md = xsu__);
	virtual V m_min(F mn);
	virtual V m_max(F mx);

	virtual V m_mixmode(BL mx) { mixmode = mx; }
	virtual V m_sigmode(BL mode) { /*dorec =*/ sigmode = mode; }
	virtual V m_loop(BL lp) { doloop = lp; }
	virtual V m_append(BL app) { if(!(appmode = app)) m_pos(0); }

	virtual V m_draw(I argc,t_atom *argv);	
protected:
	I inchns;
	BL sigmode,appmode;
	F drintv;

	BL dorec,doloop,mixmode;
	L curpos;  // in samples

	t_outlet *outmin,*outmax; // float outlets	
	
	V outputmin() { to_out_float(outmin,curmin*s2u); }
	V outputmax() { to_out_float(outmax,curmax*s2u); }
	
private:
	virtual V m_dsp(I n,F *const *in,F *const *out);
	
	DEFSIGFUN(xrecord)
	TMPLDEF V signal(I n,F *const *in,F *const *out);  // this is the dsp method

	FLEXT_CALLBACK(m_start)
	FLEXT_CALLBACK(m_stop)

	FLEXT_CALLBACK_1(m_pos,F)
	FLEXT_CALLBACK_1(m_min,F)
	FLEXT_CALLBACK_1(m_max,F)

	FLEXT_CALLBACK_B(m_loop)
	FLEXT_CALLBACK_B(m_mixmode)
	FLEXT_CALLBACK_B(m_sigmode)
	FLEXT_CALLBACK_B(m_append)

	FLEXT_CALLBACK_G(m_draw)

/*
	static V cb_start(t_class *c) { thisObject(c)->m_start(); }
	static V cb_stop(t_class *c) { thisObject(c)->m_stop(); }

	static V cb_pos(V *c,F pos) { thisObject(c)->m_pos(pos); }
	static V cb_min(V *c,F mn) { thisObject(c)->m_min(mn); }
	static V cb_max(V *c,F mx) { thisObject(c)->m_max(mx); }

	static V cb_loop(V *c,FI lp) { thisObject(c)->m_loop(lp != 0); }

	static V cb_mixmode(V *c,FI md) { thisObject(c)->m_mixmode(md != 0); }
	static V cb_sigmode(V *c,FI md) { thisObject(c)->m_sigmode(md != 0); }
	static V cb_append(V *c,FI md) { thisObject(c)->m_append(md != 0); }

	static V cb_draw(V *c,t_symbol *,I argc,t_atom *argv) { thisObject(c)->m_draw(argc,argv); }	
*/
};


FLEXT_TILDE_GIMME("xrecord~",xrecord)

V xrecord::cb_setup(t_class *c)
{
	FLEXT_ADDBANG(c,m_start);
	FLEXT_ADDMETHOD(c,"start",m_start);
	FLEXT_ADDMETHOD(c,"stop",m_stop);

	FLEXT_ADDFLOAT_N(c,2,m_min);
	FLEXT_ADDFLOAT_N(c,3,m_max);
	FLEXT_ADDMETHOD_1(c,"min",m_min,F);
	FLEXT_ADDMETHOD_1(c,"max",m_max,F);
	
	FLEXT_ADDMETHOD_B(c,"loop",m_loop);
	FLEXT_ADDMETHOD_B(c,"mixmode",m_mixmode);
	FLEXT_ADDMETHOD_B(c,"sigmode",m_sigmode);
	FLEXT_ADDMETHOD_B(c,"append",m_append);
	
	FLEXT_ADDMETHOD_G(c,"draw",m_draw);

/*
	add_floatn(c,cb_min,2);
	add_floatn(c,cb_max,3);

	add_method1(c,cb_mixmode, "mixmode", A_FLINT);	
	add_method1(c,cb_sigmode, "sigmode", A_FLINT);	
	add_method1(c,cb_loop, "loop", A_FLINT);	
	add_method1(c,cb_append, "append", A_FLINT);	
	add_method1(c,cb_min, "min", A_FLOAT);	
	add_method1(c,cb_max, "max", A_FLOAT);	

	add_bang(c,cb_start);	
	add_method(c,cb_start, "start");	
	add_method(c,cb_stop, "stop");	
	add_method1(c,cb_pos, "pos", A_FLOAT);	

	add_methodG(c,cb_draw,"draw");
*/
}

xrecord::xrecord(I argc,t_atom *argv):
	dorec(false),
	sigmode(false),mixmode(false),
	appmode(true),doloop(false),
	drintv(0)
{
#ifdef _DEBUG
	if(argc < 1) {
		post("%s - Warning: no buffer defined",thisName());
	} 
#endif

#ifdef MAXMSP
	inchns = argc >= 2?atom_getflintarg(1,argc,argv):1;
#else
	inchns = 1;
#endif

	add_in_signal(inchns);  // audio signals
	add_in_signal(); // on/off signal
	add_in_float(2);  // min & max
	add_out_signal();  // pos signal
	add_out_float(2); // min & max
	setup_inout();

	outmin = get_out(1);
	outmax = get_out(2);
	
	buf = new buffer(argc >= 1?atom_getsymbolarg(0,argc,argv):NULL,true);
#ifdef PD
	m_loadbang();  // in PD loadbang is not called upon object creation
#endif
}


V xrecord::m_units(xs_unit mode)
{
	xsample::m_units(mode);

	m_sclmode();
	outputmin();
	outputmax();
}

V xrecord::m_min(F mn)
{
	xsample::m_min(mn);
	m_pos(curpos*s2u);
	outputmin();
}

V xrecord::m_max(F mx)
{
	xsample::m_max(mx);
	m_pos(curpos*s2u);
	outputmax();
}


V xrecord::m_pos(F pos)
{
	curpos = (L)(pos/s2u+.5);
	if(curpos < curmin) curpos = curmin;
	else if(curpos > curmax) curpos = curmax;
}


I xrecord::m_set(I argc,t_atom *argv)
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units();
	return r;
}

V xrecord::m_start() 
{ 
	m_refresh(); 
	dorec = true; 
	buf->SetRefrIntv(drintv);
}

V xrecord::m_stop() 
{ 
	dorec = false; 
	if(!sigmode && !appmode) m_pos(0); 
	buf->Dirty(true);
	buf->SetRefrIntv(0);
}

V xrecord::m_reset()
{
	curpos = 0;
	xsample::m_reset();
}

V xrecord::m_draw(I argc,t_atom *argv)
{
	if(argc >= 1) {
		drintv = atom_getflintarg(0,argc,argv);
		if(dorec) buf->SetRefrIntv(drintv);
	}
	else
		buf->Dirty(true);
}
	
	
TMPLDEF V xrecord::signal(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),ICHNS,inchns);

	const F *const *sig = invecs;
	register I si = 0;
	const F *on = invecs[inchns];
	F *pos = outvecs[0];

	register const F pf = sclmul;
	register L o = curpos;
	
	if(o < curmin) o = curmin;

	if(buf && curlen > 0) {
		while(n) {
			L ncur = curmax-o; // at max to buffer or recording end

			if(ncur <= 0) {	// end of buffer
				o = curmin;
				if(doloop) 
					ncur = curlen;
				else 
					m_stop(); // loop expired;
		
			}

			if(!dorec) break;

			if(ncur > n) ncur = n;
			
			register I i;
			register F *bf = buf->Data()+o*BCHNS;
			register F p = scale(o);

			if(sigmode) {
				if(appmode) {
					// append to current position
				
					if(!mixmode) {
						for(i = 0; i < ncur; ++i,++si) {	
							if(*(on++) >= 0) {
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = sig[ci][si];	
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else 
								*(pos++) = p;
						}
					}
					else {
						for(i = 0; i < ncur; ++i,++si) {	
							register const F g = *(on++);
							if(g >= 0) {
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = bf[ci]*(1.-g)+sig[ci][si]*g;
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else 
								*(pos++) = p;
						}
					}
				}
				else {  
					// don't append
					if(!mixmode) {
						for(i = 0; i < ncur; ++i,++si) {	
							if(*(on++) >= 0)
							{ 	
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = sig[ci][si];	
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else {
								*(pos++) = p = scale(o = 0);
								bf = buf->Data();
							}
						}
					}
					else {
						for(i = 0; i < ncur; ++i,++si) {	
							register const F g = *(on++);
							if(g >= 0) {
								for(int ci = 0; ci < ICHNS; ++ci)
									bf[ci] = bf[ci]*(1.-g)+sig[ci][si]*g;
								bf += BCHNS;
								*(pos++) = p,p += pf,++o;
							}
							else {
								*(pos++) = p = scale(o = 0);
								bf = buf->Data();
							}
						}
					}
				}
			}
			else { 
				// message mode
				
				// Altivec optimization for that!
				if(!mixmode) {
					for(int ci = 0; ci < ICHNS; ++ci) {	
						register F *b = bf+ci;
						register const F *s = sig[ci];
						for(i = 0; i < ncur; ++i,b += BCHNS,++s) *b = *s;	
					}
					si += ncur;
				}
				else {
					for(i = 0; i < ncur; ++i,++si) {	
						register const F w = *(on++);
						for(int ci = 0; ci < ICHNS; ++ci)
							bf[ci] = sig[ci][si]*w;
						bf += BCHNS;
					}
				}
				for(i = 0; i < ncur; ++i) {
					*(pos++) = p,p += pf,++o;
				}
			}

			n -= ncur;
		} 
		curpos = o;
		
		buf->Dirty(); 
	}

	if(n) {
		register F p = scale(o);
		while(n--) *(pos++) = p;
	}
}

V xrecord::m_dsp(I /*n*/,F *const * /*insigs*/,F *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

	m_refresh();  

	switch(buf->Channels()*100+inchns) {
		case 101:
			sigfun = SIGFUN(xrecord,1,1);	break;
		case 102:
			sigfun = SIGFUN(xrecord,1,2);	break;
		case 201:
			sigfun = SIGFUN(xrecord,2,1);	break;
		case 202:
			sigfun = SIGFUN(xrecord,2,2);	break;
		case 401:
		case 402:
		case 403:
			sigfun = SIGFUN(xrecord,4,0);	break;
		case 404:
			sigfun = SIGFUN(xrecord,4,4);	break;
		default:
			sigfun = SIGFUN(xrecord,0,0);	break;
	}
}




V xrecord::m_help()
{
	post("%s - part of xsample objects",thisName());
	post("(C) Thomas Grill, 2001-2002 - version " XSAMPLE_VERSION " compiled on " __DATE__ " " __TIME__);
#ifdef MAXMSP
	post("Arguments: %s [buffer] [channels=1]",thisName());
#else
	post("Arguments: %s [buffer]",thisName());
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
	post("\tbang/start: start recording");
	post("\tstop: stop recording");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tsclmode 0/1/2/3: set range of position to units/units in loop/buffer/loop");
	post("\tdraw [{float}]: draw buffer immediately/set refresh time during recording (0 turns off)");
	post("");
}

V xrecord::m_print()
{
	static const C sclmode_txt[][20] = {"units","units in loop","buffer","loop"};

	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', frames = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("in channels = %i, samples/unit = %.3f, scale mode = %s",inchns,(F)(1./s2u),sclmode_txt[sclmode]); 
	post("sigmode = %s, append = %s, loop = %s, mixmode = %s",sigmode?"yes":"no",appmode?"yes":"no",doloop?"yes":"no",mixmode?"yes":"no"); 
	post("");
}


#ifdef MAXMSP
V xrecord::m_assist(L msg,L arg,C *s)
{
	switch(msg) {
	case 1: //ASSIST_INLET:
		switch(arg) {
		case 0:
			strcpy(s,"Messages and Audio signal to record"); break;
		case 1:
			strcpy(s,"On/Off/Fade/Mix signal (0..1)"); break;
		case 2:
			strcpy(s,"Starting point of recording"); break;
		case 3:
			strcpy(s,"Ending point of recording"); break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		switch(arg) {
		case 0:
			strcpy(s,"Current position of recording"); break;
		case 1:
			strcpy(s,"Starting point (rounded to sample)"); break;
		case 2:
			strcpy(s,"Ending point (rounded to sample)"); break;
		}
		break;
	}
}
#endif


