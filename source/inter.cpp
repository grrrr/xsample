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


xinter::xinter():
	doplay(false),outchns(1),
	interp(xsi_4p)
{
	FLEXT_ADDMETHOD_E(0,"interp",m_interp);
}

I xinter::m_set(I argc,t_atom *argv) 
{
	I r = xsample::m_set(argc,argv);
	if(r < 0) m_reset(); // resets pos/min/max
	if(r != 0) m_units();
	return r;
}

V xinter::m_start() 
{ 
	m_refresh(); 
	doplay = true; 
	s_dsp(); 
}

V xinter::m_stop() 
{ 
	doplay = false; 
	s_dsp(); 
}


V xinter::m_interp(xs_intp mode) 
{ 
	interp = mode; 
	s_dsp(); 
}


TMPLDEF V xinter::s_play0(I n,F *const *invecs,F *const *outvecs)
{
	// stopped
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	
	for(I ci = 0; ci < outchns; ++ci) 
		for(I si = 0; si < n; ++si) sig[ci][si] = 0;
}

TMPLDEF V xinter::s_play4(I n,F *const *invecs,F *const *outvecs)
{
	const I smin = curmin,smax = curmax,plen = curlen;
	if(plen < 4) {
		if(plen < 2) s_play1 TMPLCALL (n,invecs,outvecs);
		else s_play2 TMPLCALL (n,invecs,outvecs);
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

TMPLDEF V xinter::s_play2(I n,F *const *invecs,F *const *outvecs)
{
	const I smin = curmin,smax = curmax,plen = curlen;
	if(plen < 2) {
		s_play1 TMPLCALL (n,invecs,outvecs);
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

TMPLDEF V xinter::s_play1(I n,F *const *invecs,F *const *outvecs)
{
	SIGCHNS(BCHNS,buf->Channels(),OCHNS,outchns);

	const F *pos = invecs[0];
	F *const *sig = outvecs;
	register I si = 0;
	const I smin = curmin,smax = curmax;
	
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



V xinter::s_dsp()
{
	if(doplay) {
		if(interp == xsi_4p) 
			switch(buf->Channels()*1000+outchns) {
				case 1001:	SETSIGFUN(playfun,TMPLFUN(s_play4,1,1)); break;
				case 1002:	SETSIGFUN(playfun,TMPLFUN(s_play4,1,2)); break;
				case 2001:	SETSIGFUN(playfun,TMPLFUN(s_play4,2,1)); break;
				case 2002:	SETSIGFUN(playfun,TMPLFUN(s_play4,2,2)); break;
				case 4001:
				case 4002:
				case 4003:	SETSIGFUN(playfun,TMPLFUN(s_play4,4,-1)); break;
				case 4004:	SETSIGFUN(playfun,TMPLFUN(s_play4,4,4)); break;
				default:	SETSIGFUN(playfun,TMPLFUN(s_play4,-1,-1));
			}
		else if(interp == xsi_lin) 
			switch(buf->Channels()*1000+outchns) {
				case 1001:	SETSIGFUN(playfun,TMPLFUN(s_play2,1,1)); break;
				case 1002:	SETSIGFUN(playfun,TMPLFUN(s_play2,1,2)); break;
				case 2001:	SETSIGFUN(playfun,TMPLFUN(s_play2,2,1)); break;
				case 2002:	SETSIGFUN(playfun,TMPLFUN(s_play2,2,2)); break;
				case 4001:
				case 4002:
				case 4003:	SETSIGFUN(playfun,TMPLFUN(s_play2,4,-1)); break;
				case 4004:	SETSIGFUN(playfun,TMPLFUN(s_play2,4,4)); break;
				default:	SETSIGFUN(playfun,TMPLFUN(s_play2,-1,-1));
			}
		else 
			switch(buf->Channels()*1000+outchns) {
				case 1001:	SETSIGFUN(playfun,TMPLFUN(s_play1,1,1)); break;
				case 1002:	SETSIGFUN(playfun,TMPLFUN(s_play1,1,2)); break;
				case 2001:	SETSIGFUN(playfun,TMPLFUN(s_play1,2,1)); break;
				case 2002:	SETSIGFUN(playfun,TMPLFUN(s_play1,2,2)); break;
				case 4001:
				case 4002:
				case 4003:	SETSIGFUN(playfun,TMPLFUN(s_play1,4,-1)); break;
				case 4004:	SETSIGFUN(playfun,TMPLFUN(s_play1,4,4)); break;
				default:	SETSIGFUN(playfun,TMPLFUN(s_play1,-1,-1));
			}
	}
	else
		SETSIGFUN(playfun,TMPLFUN(s_play0,-1,-1));
}
