/*

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001,2002 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"


// Initialization function for xsample library
V lib_setup()
{
	post("xsample objects, version " XSAMPLE_VERSION ", (C)2001,2002 Thomas Grill");
	post("xsample: xrecord~, xplay~, xgroove~ - send objects a 'help' message to get assistance");
	post("");

	// call the objects' setup routines
	FLEXT_DSP_SETUP(xrecord);
	FLEXT_DSP_SETUP(xplay);
	FLEXT_DSP_SETUP(xgroove);
	
#ifdef MAXMSP
	// That's not the right place..... (doesn't work)
	finder_addclass((char *)"MSP Sampling",(char *)"xgroove~");
	finder_addclass((char *)"MSP Sampling",(char *)"xplay~");
	finder_addclass((char *)"MSP Sampling",(char *)"xrecord~");
#endif

}

// setup the library
FLEXT_LIB_SETUP(xsample,lib_setup)

// ------------------------------

xsample::xsample():
	buf(NULL),
#ifdef PD
	unitmode(xsu_sample),  // PD defaults to samples
#else
	unitmode(xsu_ms),	   // Max/MSP defaults to milliseconds
#endif
	sclmode(xss_unitsinbuf),
	curmin(0),curmax(1<<30)
{
	FLEXT_ADDBANG(0,m_start);
	FLEXT_ADDMETHOD_(0,"start",m_start);
	FLEXT_ADDMETHOD_(0,"stop",m_stop);

	FLEXT_ADDMETHOD_(0,"set",m_set);
	FLEXT_ADDMETHOD_(0,"print",m_print);
	FLEXT_ADDMETHOD_(0,"refresh",m_refresh);
	FLEXT_ADDMETHOD_(0,"reset",m_reset);

	FLEXT_ADDMETHOD_E(0,"units",m_units);
	FLEXT_ADDMETHOD_E(0,"sclmode",m_sclmode);
}
	
xsample::~xsample()
{
//	m_enable(false); // switch off DSP

	if(buf) delete buf; 
}



I xsample::m_set(I argc,const t_atom *argv)
{
	return buf->Set(argc >= 1?GetASymbol(argv[0]):NULL);
}

BL xsample::m_refresh()
{
//	bufchk();

	BL ret;
	if(buf->Set()) { s_dsp(); ret = true; } // channel count may have changed
	else ret = false;
	
	m_min((F)curmin*s2u); // also checks pos
	m_max((F)curmax*s2u); // also checks pos

	return ret;
}

BL xsample::m_reset()
{
//	bufchk();

	BL ret;
	if(buf->Set()) { s_dsp(); ret = true; } // channel count may have changed
	else ret = false;
	
	m_units();
	m_sclmode();	
	m_min(0);
    m_max(buf->Frames()*s2u);

	return ret;
}

V xsample::m_loadbang() 
{
	m_reset();
}

V xsample::m_units(xs_unit mode)
{
	bufchk();

	if(mode != xsu__) unitmode = mode;
	switch(unitmode) {
		case xsu_sample: // samples
			s2u = 1;
			break;
		case xsu_buffer: // buffer size
			s2u = 1.f/buf->Frames();
			break;
		case xsu_ms: // ms
			s2u = 1000.f/Samplerate();
			break;
		case xsu_s: // s
			s2u = 1.f/Samplerate();
			break;
		default:
			post("%s: Unknown unit mode",thisName());
	}
}

V xsample::m_sclmode(xs_sclmd mode)
{
	bufchk();

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
//			sclmin = curmin; sclmul = curlen?1.f/curlen:0;
			sclmin = curmin; sclmul = curmin != curmax?1.f/(curmax-curmin):0;
			break;
		default:
			post("%s: Unknown scale mode",thisName());
	}
}

V xsample::m_min(F mn)
{
	bufchk();

	mn /= s2u;  // conversion to samples
	if(mn < 0) mn = 0;
	else if(mn > curmax) mn = (F)curmax;
	curmin = (I)(mn+.5);
//	curlen = curmax-curmin;

	m_sclmode();
}

V xsample::m_max(F mx)
{
	bufchk();

	mx /= s2u;  // conversion to samples
	if(mx > buf->Frames()) mx = (F)buf->Frames();
	else if(mx < curmin) mx = (F)curmin;
	curmax = (I)(mx+.5);
//	curlen = curmax-curmin;

	m_sclmode();
}

V xsample::m_all()
{
	bufchk();

//	curlen = (curmax = buf->Frames())-(curmin = 0);
	curmin = 0; curmax = buf->Frames();
	m_sclmode();
}

V xsample::m_dsp(I /*n*/,S *const * /*insigs*/,S *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

	if(!m_refresh()) s_dsp();
}




