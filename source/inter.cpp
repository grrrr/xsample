/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2004 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
#include <math.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

#ifndef TMPLOPT
#include "inter.h"
#endif

void xinter::setup(t_classid c)
{
	FLEXT_CADDBANG(c,0,m_start);
	FLEXT_CADDMETHOD_(c,0,"start",m_start);
	FLEXT_CADDMETHOD_(c,0,"stop",m_stop);

	FLEXT_CADDATTR_VAR_E(c,"interp",interp,m_interp);
}

/*
I xinter::m_set(I argc,const t_atom *argv) 
{
	I r = xsample::m_set(argc,argv);
	if(r) 
        // buffer parameters have changed, reset pos/min/max
        m_reset(); 
	return r;
}
*/

void xinter::m_start() 
{ 
	ChkBuffer();
    doplay = true;
    Update(xsc_startstop);
    DoUpdate();
}

void xinter::m_stop() 
{ 
	ChkBuffer();
    doplay = false;
    Update(xsc_startstop);
    DoUpdate();
}

void xinter::SetPlay()
{
    xsample::SetPlay();

	switch(outchns) {
		case 1:	SETSIGFUN(zerofun,TMPLFUN(s_play0,-1,1)); break;
		case 2:	SETSIGFUN(zerofun,TMPLFUN(s_play0,-1,2)); break;
		case 4:	SETSIGFUN(zerofun,TMPLFUN(s_play0,-1,4)); break;
		default:	SETSIGFUN(zerofun,TMPLFUN(s_play0,-1,-1));
	}

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


