/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2003 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#ifndef __INTER_H
#define __INTER_H

TMPLDEF V xinter::st_play0(const S *,const I ,const I ,const I n,const I inchns,const I outchns,S *const *invecs,S *const *outvecs)
{
	// stopped/invalid buffer -> output zero
	for(I ci = 0; ci < outchns; ++ci) ZeroSamples(outvecs[ci],n);
}

TMPLDEF V xinter::st_play1(const S *bdt,const I smin,const I smax,const I n,const I inchns,const I outchns,S *const *invecs,S *const *outvecs)
{
	SIGCHNS(BCHNS,inchns,OCHNS,outchns);

	// position info are frame units
	const S *pos = invecs[0]; 
	S *const *sig = outvecs;
	register I si = 0;
	
	// no interpolation
	// ----------------
	
	for(I i = 0; i < n; ++i,++si) {	
		register const I oint = (I)(*(pos++));
		register const S *fp;
		if(oint < smin) {
			// position < 0 ... take only 0th sample
			fp = bdt+smin*BCHNS;
		}
		else if(oint >= smax) {
			// position > last sample ... take only last sample
			fp = bdt+(smin == smax?smin:smax-1)*BCHNS;
		}
		else {
			// normal
			fp = bdt+oint*BCHNS;
		}

		for(I ci = 0; ci < OCHNS; ++ci)
			sig[ci][si] = fp[ci];
	}

	// clear rest of output channels (if buffer has less channels)
	for(I ci = OCHNS; ci < outchns; ++ci) ZeroSamples(sig[ci],n);
}

TMPLDEF V xinter::st_play2(const S *bdt,const I smin,const I smax,const I n,const I inchns,const I outchns,S *const *invecs,S *const *outvecs)
{
	const I plen = smax-smin; //curlen;
	if(plen < 2) {
		st_play1 TMPLCALL (bdt,smin,smax,n,inchns,outchns,invecs,outvecs);
		return;
	}

	SIGCHNS(BCHNS,inchns,OCHNS,outchns);

	// position info are frame units
	const S *pos = invecs[0];
	S *const *sig = outvecs;
	register I si = 0;
	
	// linear interpolation
	// --------------------

	const I maxo = smax-1;  // last sample in buffer

	for(I i = 0; i < n; ++i,++si) {	
		const F o = *(pos++);
		register const I oint = (I)o;

		if(oint < smin) {
			// position is before first sample -> take the first sample
			register const S *const fp = bdt+smin*BCHNS;
			for(I ci = 0; ci < OCHNS; ++ci) 
				sig[ci][si] = fp[ci]; 
		}
		else if(oint >= maxo) {
			// position is past last sample -> take the last sample
			register const S *const fp = bdt+maxo*BCHNS;
			for(I ci = 0; ci < OCHNS; ++ci) 
				sig[ci][si] = fp[ci]; 
		}
		else {
			// normal interpolation
			register const F frac = o-oint;
			register const S *const fp0 = bdt+oint*BCHNS;
			register const S *const fp1 = fp0+BCHNS;
			for(I ci = 0; ci < OCHNS; ++ci) 
				sig[ci][si] = fp0[ci]+frac*(fp1[ci]-fp0[ci]);
		}
	}

	// clear rest of output channels (if buffer has less channels)
	for(I ci = OCHNS; ci < outchns; ++ci) ZeroSamples(sig[ci],n);
}

TMPLDEF V xinter::st_play4(const S *bdt,const I smin,const I smax,const I n,const I inchns,const I outchns,S *const *invecs,S *const *outvecs)
{
	const I plen = smax-smin; //curlen;
	if(plen < 4) {
		if(plen < 2) st_play1 TMPLCALL (bdt,smin,smax,n,inchns,outchns,invecs,outvecs);
		else st_play2 TMPLCALL (bdt,smin,smax,n,inchns,outchns,invecs,outvecs);
		return;
	}

	SIGCHNS(BCHNS,inchns,OCHNS,outchns);

	// position info are frame units
	const S *pos = invecs[0];

#ifdef __VEC__
	// prefetch cache
	vec_dst(pos,GetPrefetchConstant(1,n>>2,0),0);
	const int pf = GetPrefetchConstant(BCHNS,1,16*BCHNS);
#endif

	S *const *sig = outvecs;
	register I si = 0;
	
	// 4-point interpolation
	// ---------------------
	const I maxo = smax-1; // last sample in play region
	const S *maxp = bdt+maxo*BCHNS;  // pointer to last sample

	for(I i = 0; i < n; ++i,++si) {	
		F o = *(pos++);
		register I oint = (I)o;
		register F frac;
		register const S *fa,*fb,*fc,*fd;

		if(oint <= smin) { 
			// if oint < first sample set it to first sample
			// \TODO what about wraparound (in loop/palindrome mode) ?
			if(oint < smin) oint = smin,o = (float)smin;
			
			// position is first sample
			fa = bdt+smin*BCHNS;  

			fb = bdt+oint*BCHNS;
			frac = o-oint;
			fc = fb+BCHNS;
			fd = fc+BCHNS;
		}
		else if(oint >= maxo-2) { 
			if(oint > maxo) oint = maxo,o = (float)smax;
			frac = o-oint;

			fb = bdt+oint*BCHNS;
			fa = fb-BCHNS;   
			
			// \TODO what about wraparound (in loop/palindrome mode) ?
			fc = fb >= maxp?maxp:fb+BCHNS;
			fd = fc >= maxp?maxp:fc+BCHNS;
		}
		else {
			fa = bdt+oint*BCHNS-BCHNS;
			frac = o-oint;
			fb = fa+BCHNS;
#ifdef __VEC__
			vec_dst(fa,pf,0);
#endif
			fc = fb+BCHNS;
			fd = fc+BCHNS;
		}

		register F f1 = 0.5f*(frac-1.0f);
		register F f3 = frac*3.0f-1.0f;
		
		for(I ci = 0; ci < OCHNS; ++ci) {
			const F amdf = (fa[ci]-fd[ci])*frac;
			const F cmb = fc[ci]-fb[ci];
			const F bma = fb[ci]-fa[ci];
			sig[ci][si] = fb[ci] + frac*( cmb - f1 * ( amdf+bma+cmb*f3 ) );
		}
	}
	
#ifdef __VEC__
	vec_dss(0);
#endif

	// clear rest of output channels (if buffer has less channels)
	for(I ci = OCHNS; ci < outchns; ++ci) ZeroSamples(sig[ci],n);
}


TMPLDEF inline V xinter::s_play0(I n,S *const *invecs,S *const *outvecs)
{
	st_play0 TMPLCALL (buf->Data(),curmin,curmax,n,buf->Channels(),outchns,invecs,outvecs);
}

TMPLDEF inline V xinter::s_play1(I n,S *const *invecs,S *const *outvecs)
{
	st_play1 TMPLCALL (buf->Data(),curmin,curmax,n,buf->Channels(),outchns,invecs,outvecs);
}

TMPLDEF inline V xinter::s_play2(I n,S *const *invecs,S *const *outvecs)
{
	st_play2 TMPLCALL (buf->Data(),curmin,curmax,n,buf->Channels(),outchns,invecs,outvecs);
}

TMPLDEF inline V xinter::s_play4(I n,S *const *invecs,S *const *outvecs)
{
	st_play4 TMPLCALL (buf->Data(),curmin,curmax,n,buf->Channels(),outchns,invecs,outvecs);
}

#endif
