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
			fp = bdt+(smax-1)*BCHNS;
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
	S *const *sig = outvecs;
	register I si = 0;
	
	// 4-point interpolation
	// ---------------------
	const I maxo = smax-1; // last sample in play region

	for(I i = 0; i < n; ++i,++si) {	
		F o = *(pos++);
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
		
		register const S *fa = bdt+ointm*BCHNS;
		register const S *fb = bdt+oint*BCHNS;
		register const S *fc = bdt+oint1*BCHNS;
		register const S *fd = bdt+oint2*BCHNS;

		for(I ci = 0; ci < OCHNS; ++ci) {
			const F cmb = fc[ci]-fb[ci];
			sig[ci][si] = fb[ci] + frac*( 
				cmb - 0.5f*(frac-1.0f) * ((fa[ci]-fd[ci]+3.0f*cmb)*frac + (fb[ci]-fa[ci]-cmb))
			);
		}
	}

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
