/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2003 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"
#include <math.h>

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

#ifndef TMPLOPT
#include "inter.ci"
#endif

void xinter::setup(t_classid c)
{
	FLEXT_CADDATTR_VAR_E(c,"interp",interp,m_interp);
}

xinter::xinter():
	doplay(false),outchns(1),
	interp(xsi_4p)
{
}

I xinter::m_set(I argc,const t_atom *argv) 
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


