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


class xplay:
	public xsample
{
//	FLEXT_HEADER_S(xplay,xsample,setup)
	FLEXT_HEADER(xplay,xsample)

public:
	xplay(I argc, t_atom *argv);
	
#ifdef MAXMSP
	virtual V m_assist(L msg,L arg,C *s);
#endif

	virtual V m_help();
	virtual V m_print();
	
	virtual I m_set(I argc,t_atom *argv);

	virtual V m_start();
	virtual V m_stop();

protected:
	BL doplay;
	I outchns;

private:
	static V setup(t_class *c);

	virtual V s_dsp();

	DEFSIGFUN(xplay)
	TMPLDEF V signal(I n,F *const *in,F *const *out);  // this is the dsp method
	TMPLDEF V signal0(I n,F *const *in,F *const *out);  // this is the dsp method
	TMPLDEF V signal1(I n,F *const *in,F *const *out);  // this is the dsp method
	TMPLDEF V signal2(I n,F *const *in,F *const *out);  // this is the dsp method
	TMPLDEF V signal4(I n,F *const *in,F *const *out);  // this is the dsp method
};

FLEXT_LIB_TILDE_G("xplay~",xplay)

/*
V xplay::setup(t_class *)
{
#ifndef PD
	post("loaded xplay~ - part of xsample objects, version " XSAMPLE_VERSION " - (C) Thomas Grill, 2001-2002");
#endif
}
*/

xplay::xplay(I argc, t_atom *argv): 
	doplay(false),
	outchns(1)
{
	I argi = 0;
#ifdef MAXMSP
	if(argc > argi && IsFlint(argv[argi])) {
		outchns = GetAFlint(argv[argi]);
		argi++;
	}
#endif

	if(argc > argi && IsSymbol(argv[argi])) {
		buf = new buffer(GetSymbol(argv[argi]),true);
		argi++;
		
#ifdef MAXMSP
		// oldstyle command line?
		if(argi == 1 && argc == 2 && IsFlint(argv[argi])) {
			outchns = GetAFlint(argv[argi]);
			argi++;
			post("%s: old style command line detected - please change to '%s [channels] [buffer]'",thisName(),thisName()); 
		}
#endif
	}
	else
		buf = new buffer(NULL,true);

	AddInSignal();  // pos signal
	AddOutSignal(outchns);
	SetupInOut();
}


I xplay::m_set(I argc,t_atom *argv) 
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units();
	return r;
}

V xplay::m_start() 
{ 
	doplay = true; 
	s_dsp(); 
}

V xplay::m_stop() 
{ 
	doplay = false; 
	s_dsp(); 
}

/*
TMPLDEF V xplay::signal(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	register I si = 0;
	const I smin = 0,smax = buf->Frames(),plen = smax-smin;
	
	if(doplay && buf->Frames() > 0) {
		if(interp == xsi_4p && plen >= 4) {
			// 4-point interpolation
			// ---------------------
			
			const I maxo = smax-1; // last sample in play region

			for(I i = 0; i < n; ++i,++si) {	
				F o = *(pos++)/s2u;
				register I oint = (I)o,ointm,oint1,oint2;

				if(oint <= smin) { 
					if(oint < smin) oint = smin,o = smin;
					// position is first simple
					ointm = smin; // first sample 
					oint1 = oint+1;
					oint2 = oint1+1;
				}
				else if(oint >= maxo-2) { 
					if(oint > maxo) oint = maxo,o = smax;
					ointm = oint-1;
					oint1 = oint >= maxo?maxo:oint+1;
					oint2 = oint1 >= maxo?maxo:oint1+1;
				}
				else {
					ointm = oint-1;
					oint1 = oint+1;
					oint2 = oint1+1;
				}

				register F frac = o-oint;
				
				register F *fa = buf->Data()+ointm*BCHNS;
				register F *fb = buf->Data()+oint*BCHNS;
				register F *fc = buf->Data()+oint1*BCHNS;
				register F *fd = buf->Data()+oint2*BCHNS;

				for(I ci = 0; ci < OCHNS; ++ci) {
					const F cmb = fc[ci]-fb[ci];
					sig[ci][si] = fb[ci] + frac*( 
						cmb - 0.5f*(frac-1.) * ((fa[ci]-fd[ci]+3.0f*cmb)*frac + (fb[ci]-fa[ci]-cmb))
					);
				}
			}
		}
		else if(interp == xsi_lin && plen >= 2) {
			// linear interpolation
			// --------------------
		
			const I maxo = smax-1;  // last sample in buffer

			for(I i = 0; i < n; ++i,++si) {	
				const F o = *(pos++)/s2u;
				register const I oint = (I)o;

				if(oint < smin) {
					// position is before first sample -> take the first sample
					register const F *const fp = buf->Data()+smin*BCHNS;
					for(I ci = 0; ci < OCHNS; ++ci) 
						sig[ci][si] = fp[ci]; 
				}
				else if(oint >= maxo) {
					// position is past last sample -> take the last sample
					register const F *const fp = buf->Data()+maxo*BCHNS;
					for(I ci = 0; ci < OCHNS; ++ci) 
						sig[ci][si] = fp[ci]; 
				}
				else {
					// normal interpolation
					register const F frac = o-oint;
					register const F *const fp0 = buf->Data()+oint*BCHNS;
					register const F *const fp1 = fp0+BCHNS;
					for(I ci = 0; ci < OCHNS; ++ci) 
						sig[ci][si] = fp0[ci]+frac*(fp1[ci]-fp0[ci]);
				}
			}
		}
		else {
			// no interpolation
			// ----------------
			
			for(I i = 0; i < n; ++i,++si) {	
				register const I oint = (I)(*(pos++)/s2u);
				register F *fp;
				if(oint < smin) {
					// position < 0 ... take only 0th sample
					fp = buf->Data()+smin*BCHNS;
				}
				else if(oint >= smax) {
					// position > last sample ... take only last sample
					fp = buf->Data()+(smax-1)*BCHNS;
				}
				else {
					// normal
					fp = buf->Data()+oint*BCHNS;
				}

				for(I ci = 0; ci < OCHNS; ++ci)
					sig[ci][si] = fp[ci];
			}
		}	

		// clear rest of output channels (if buffer has less channels)
		for(I ci = OCHNS; ci < outchns; ++ci) 
			for(I i = 0; i < n; ++i) sig[ci][i] = 0;
	}
	else {
		for(I ci = 0; ci < outchns; ++ci) 
			for(I si = 0; si < n; ++si) sig[ci][si] = 0;
	}
}
*/

TMPLDEF V xplay::signal0(I n,F *const *invecs,F *const *outvecs)
{
	// stopped
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	
	for(I ci = 0; ci < outchns; ++ci) 
		for(I si = 0; si < n; ++si) sig[ci][si] = 0;
}

TMPLDEF V xplay::signal4(I n,F *const *invecs,F *const *outvecs)
{
	const I smin = 0,smax = buf->Frames(),plen = smax-smin;
	if(plen < 4) {
		if(plen < 2) xplay::signal1(n,invecs,outvecs);
		else xplay::signal2(n,invecs,outvecs);
		return;
	}

	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	register I si = 0;
	
	// 4-point interpolation
	// ---------------------
	const I maxo = smax-1; // last sample in play region

	for(I i = 0; i < n; ++i,++si) {	
		F o = *(pos++)/s2u;
		register I oint = (I)o,ointm,oint1,oint2;

		if(oint <= smin) { 
			if(oint < smin) oint = smin,o = smin;
			// position is first simple
			ointm = smin; // first sample 
			oint1 = oint+1;
			oint2 = oint1+1;
		}
		else if(oint >= maxo-2) { 
			if(oint > maxo) oint = maxo,o = smax;
			ointm = oint-1;
			oint1 = oint >= maxo?maxo:oint+1;
			oint2 = oint1 >= maxo?maxo:oint1+1;
		}
		else {
			ointm = oint-1;
			oint1 = oint+1;
			oint2 = oint1+1;
		}

		register F frac = o-oint;
		
		register F *fa = buf->Data()+ointm*BCHNS;
		register F *fb = buf->Data()+oint*BCHNS;
		register F *fc = buf->Data()+oint1*BCHNS;
		register F *fd = buf->Data()+oint2*BCHNS;

		for(I ci = 0; ci < OCHNS; ++ci) {
			const F cmb = fc[ci]-fb[ci];
			sig[ci][si] = fb[ci] + frac*( 
				cmb - 0.5f*(frac-1.) * ((fa[ci]-fd[ci]+3.0f*cmb)*frac + (fb[ci]-fa[ci]-cmb))
			);
		}
	}

	// clear rest of output channels (if buffer has less channels)
	for(I ci = OCHNS; ci < outchns; ++ci) 
		for(si = 0; si < n; ++si) sig[ci][si] = 0;
}

TMPLDEF V xplay::signal2(I n,F *const *invecs,F *const *outvecs)
{
	const I smin = 0,smax = buf->Frames(),plen = smax-smin;
	if(plen < 2) {
		xplay::signal1(n,invecs,outvecs);
		return;
	}

	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	register I si = 0;
	
	// linear interpolation
	// --------------------

	const I maxo = smax-1;  // last sample in buffer

	for(I i = 0; i < n; ++i,++si) {	
		const F o = *(pos++)/s2u;
		register const I oint = (I)o;

		if(oint < smin) {
			// position is before first sample -> take the first sample
			register const F *const fp = buf->Data()+smin*BCHNS;
			for(I ci = 0; ci < OCHNS; ++ci) 
				sig[ci][si] = fp[ci]; 
		}
		else if(oint >= maxo) {
			// position is past last sample -> take the last sample
			register const F *const fp = buf->Data()+maxo*BCHNS;
			for(I ci = 0; ci < OCHNS; ++ci) 
				sig[ci][si] = fp[ci]; 
		}
		else {
			// normal interpolation
			register const F frac = o-oint;
			register const F *const fp0 = buf->Data()+oint*BCHNS;
			register const F *const fp1 = fp0+BCHNS;
			for(I ci = 0; ci < OCHNS; ++ci) 
				sig[ci][si] = fp0[ci]+frac*(fp1[ci]-fp0[ci]);
		}
	}

	// clear rest of output channels (if buffer has less channels)
	for(I ci = OCHNS; ci < outchns; ++ci) 
		for(si = 0; si < n; ++si) sig[ci][si] = 0;
}

TMPLDEF V xplay::signal1(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	register I si = 0;
	const I smin = 0,smax = buf->Frames(),plen = smax-smin;
	
	// no interpolation
	// ----------------
	
	for(I i = 0; i < n; ++i,++si) {	
		register const I oint = (I)(*(pos++)/s2u);
		register F *fp;
		if(oint < smin) {
			// position < 0 ... take only 0th sample
			fp = buf->Data()+smin*BCHNS;
		}
		else if(oint >= smax) {
			// position > last sample ... take only last sample
			fp = buf->Data()+(smax-1)*BCHNS;
		}
		else {
			// normal
			fp = buf->Data()+oint*BCHNS;
		}

		for(I ci = 0; ci < OCHNS; ++ci)
			sig[ci][si] = fp[ci];
	}

	// clear rest of output channels (if buffer has less channels)
	for(I ci = OCHNS; ci < outchns; ++ci) 
		for(si = 0; si < n; ++si) sig[ci][si] = 0;
}

V xplay::s_dsp()
{
	if(doplay) {
		if(interp == xsi_4p) 
			switch(buf->Channels()*1000+outchns) {
				case 1001:	sigfun = SIGFUN(xplay,signal4,1,1); break;
				case 1002:	sigfun = SIGFUN(xplay,signal4,1,2); break;
				case 2001:	sigfun = SIGFUN(xplay,signal4,2,1); break;
				case 2002:	sigfun = SIGFUN(xplay,signal4,2,2); break;
				case 4001:
				case 4002:
				case 4003:	sigfun = SIGFUN(xplay,signal4,4,-1); break;
				case 4004:	sigfun = SIGFUN(xplay,signal4,4,4); break;
				default:	sigfun = SIGFUN(xplay,signal4,-1,-1);
			}
		else if(interp == xsi_lin) 
			switch(buf->Channels()*1000+outchns) {
				case 1001:	sigfun = SIGFUN(xplay,signal2,1,1); break;
				case 1002:	sigfun = SIGFUN(xplay,signal2,1,2); break;
				case 2001:	sigfun = SIGFUN(xplay,signal2,2,1); break;
				case 2002:	sigfun = SIGFUN(xplay,signal2,2,2); break;
				case 4001:
				case 4002:
				case 4003:	sigfun = SIGFUN(xplay,signal2,4,-1); break;
				case 4004:	sigfun = SIGFUN(xplay,signal2,4,4); break;
				default:	sigfun = SIGFUN(xplay,signal2,-1,-1);
			}
		else 
			switch(buf->Channels()*1000+outchns) {
				case 1001:	sigfun = SIGFUN(xplay,signal1,1,1); break;
				case 1002:	sigfun = SIGFUN(xplay,signal1,1,2); break;
				case 2001:	sigfun = SIGFUN(xplay,signal1,2,1); break;
				case 2002:	sigfun = SIGFUN(xplay,signal1,2,2); break;
				case 4001:
				case 4002:
				case 4003:	sigfun = SIGFUN(xplay,signal1,4,-1); break;
				case 4004:	sigfun = SIGFUN(xplay,signal1,4,4); break;
				default:	sigfun = SIGFUN(xplay,signal1,-1,-1);
			}
	}
	else
		sigfun = SIGFUN(xplay,signal0,-1,-1);
}



V xplay::m_help()
{
	post("%s - part of xsample objects, version " XSAMPLE_VERSION,thisName());
#ifdef _DEBUG
	post("compiled on " __DATE__ " " __TIME__);
#endif
	post("(C) Thomas Grill, 2001-2002");
#ifdef MAXMSP
	post("Arguments: %s [channels=1] [buffer]",thisName());
#else
	post("Arguments: %s [buffer]",thisName());
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
	post("\treset: checks buffer");
	post("\trefresh: checks buffer and refreshes outlets");
	post("\tunits 0/1/2/3: set units to samples/buffer size/ms/s");
	post("\tinterp 0/1/2: set interpolation to off/4-point/linear");
	post("");
}

V xplay::m_print()
{
	const C *interp_txt[] = {"off","4-point","linear"};
	// print all current settings
	post("%s - current settings:",thisName());
	post("bufname = '%s', length = %.3f, channels = %i",buf->Name(),(F)(buf->Frames()*s2u),buf->Channels()); 
	post("out channels = %i, samples/unit = %.3f, interpolation = %s",outchns,(F)(1./s2u),interp_txt[interp >= xsi_none && interp <= xsi_lin?interp:xsi_none]); 
	post("");
}


#ifdef MAXMSP
V xplay::m_assist(L msg,L arg,C *s)
{
	switch(msg) {
	case 1: //ASSIST_INLET:
		switch(arg) {
		case 0:
			sprintf(s,"Messages and Signal of playing position"); break;
		}
		break;
	case 2: //ASSIST_OUTLET:
		if(arg < outchns) 
			sprintf(s,"Audio signal channel %li",arg+1); 
		break;
	}
}
#endif



