/* 

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"


#ifdef PD
// Initialization for pd xsample library

extern "C" {
FLEXT_EXT V xrecord_tilde_setup();
FLEXT_EXT V xplay_tilde_setup();
FLEXT_EXT V xgroove_tilde_setup();

FLEXT_EXT V xsample_setup()
{
	post("xsample objects, version " XSAMPLE_VERSION ", (C)2001,2002 Thomas Grill");
	post("xsample: xrecord~, xplay~, xgroove~ - send objects a 'help' message to get assistance");
	post("");

	// call the objects' setup routines
	xrecord_tilde_setup();
	xplay_tilde_setup();
	xgroove_tilde_setup();
}
}
#endif

// ------------------------------

xsample::xsample():
	buf(NULL),
#ifdef PD
	unitmode(xsu_sample),  // PD defaults to samples
#else
	unitmode(xsu_ms),	   // Max/MSP defaults to milliseconds
#endif
	interp(xsi_4p),
	sclmode(xss_unitsinbuf),
	curmin(0),curmax(1<<30)
{
	FLEXT_ADDMETHOD_(0,"set",m_set);
	FLEXT_ADDMETHOD_(0,"print",m_print);
	FLEXT_ADDMETHOD_(0,"refresh",m_refresh);
	FLEXT_ADDMETHOD_(0,"reset",m_reset);

	FLEXT_ADDMETHOD_E(0,"units",m_units);
	FLEXT_ADDMETHOD_E(0,"interp",m_interp);
	FLEXT_ADDMETHOD_E(0,"sclmode",m_sclmode);
}
	
xsample::~xsample()
{
	if(buf) delete buf; 
}



I xsample::m_set(I argc, t_atom *argv)
{
	return buf->Set(argc >= 1?geta_symbol(argv[0]):NULL);
}

V xsample::m_refresh()
{
	if(buf->Set())	
		m_dsp(0,NULL,NULL); // channel count may have changed
	
	m_min((F)curmin*s2u); // also checks pos
	m_max((F)curmax*s2u); // also checks pos
}

V xsample::m_reset()
{
	if(buf->Set())	
		m_dsp(0,NULL,NULL); // channel count may have changed
	
	m_min(0);
    m_max(buf->Frames()*s2u);
	m_units();
	m_sclmode();	
}

V xsample::m_loadbang() 
{
	m_reset();
}

V xsample::m_units(xs_unit mode)
{
	if(mode != xsu__) unitmode = mode;
	switch(unitmode) {
		case xsu_sample: // samples
			s2u = 1;
			break;
		case xsu_buffer: // buffer size
			s2u = 1.f/buf->Frames();
			break;
		case xsu_ms: // ms
			s2u = 1000.f/sys_getsr();
			break;
		case xsu_s: // s
			s2u = 1.f/sys_getsr();
			break;
		default:
			post("%s: Unknown unit mode",thisName());
	}
}

V xsample::m_interp(xs_intp mode) { interp = mode; }

V xsample::m_sclmode(xs_sclmd mode)
{
	if(mode != xss__) sclmode = mode;
	switch(sclmode) {
		case 0: // samples/units
			sclmin = 0; sclmul = s2u;
			break;
		case 1: // samples/units from recmin to recmax
			sclmin = curmin; sclmul = s2u;
			break;
		case 2: // unity between 0 and buffer size
			sclmin = 0; sclmul = buf->Frames()?1.f/buf->Frames():0;
			break;
		case 3:	// unity between recmin and recmax
			sclmin = curmin; sclmul = curlen?1.f/curlen:0;
			break;
		default:
			post("%s: Unknown scale mode",thisName());
	}
}

V xsample::m_min(F mn)
{
	mn /= s2u;  // conversion to samples
	if(mn < 0) mn = 0;
	else if(mn > curmax) mn = (F)curmax;
	curmin = (I)(mn+.5);
	curlen = curmax-curmin;

	m_sclmode();
}

V xsample::m_max(F mx)
{
	mx /= s2u;  // conversion to samples
	if(mx > buf->Frames()) mx = (F)buf->Frames();
	else if(mx < curmin) mx = (F)curmin;
	curmax = (I)(mx+.5);
	curlen = curmax-curmin;

	m_sclmode();
}



