/*

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2003 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"


// Initialization function for xsample library
static V xsample_main()
{
	post("-------------------------------");
	post("xsample objects, version " XSAMPLE_VERSION);
    post("");
	post("  xrecord~, xplay~, xgroove~   ");
    post("  (C)2001-2003 Thomas Grill    ");
#ifdef FLEXT_DEBUG
    post("          DEBUG BUILD          ");
#endif
	post("-------------------------------");

	// call the objects' setup routines
	FLEXT_DSP_SETUP(xrecord);
	FLEXT_DSP_SETUP(xplay);
	FLEXT_DSP_SETUP(xgroove);
	
#if FLEXT_SYS == FLEXT_SYS_MAX
	finder_addclass((C *)"MSP Sampling",(C *)"xgroove~");
	finder_addclass((C *)"MSP Sampling",(C *)"xplay~");
	finder_addclass((C *)"MSP Sampling",(C *)"xrecord~");
#endif

}

// setup the library
FLEXT_LIB_SETUP(xsample,xsample_main)

// ------------------------------

void xsample::setup(t_classid c)
{
	FLEXT_CADDBANG(c,0,m_start);
	FLEXT_CADDMETHOD_(c,0,"start",m_start);
	FLEXT_CADDMETHOD_(c,0,"stop",m_stop);

	FLEXT_CADDMETHOD_(c,0,"set",m_set);
	FLEXT_CADDMETHOD_(c,0,"print",m_print);
	FLEXT_CADDMETHOD_(c,0,"refresh",m_refresh);
	FLEXT_CADDMETHOD_(c,0,"reset",m_reset);

	FLEXT_CADDATTR_VAR(c,"buffer",mg_buffer,ms_buffer);
	FLEXT_CADDATTR_VAR_E(c,"units",unitmode,m_units);
	FLEXT_CADDATTR_VAR_E(c,"sclmode",sclmode,m_sclmode);
	FLEXT_CADDATTR_GET(c,"scale",s2u);
}

xsample::xsample():
	buf(NULL),
#if FLEXT_SYS == FLEXT_SYS_MAX
	unitmode(xsu_ms),	   // Max/MSP defaults to milliseconds
#else
	unitmode(xsu_sample),  // PD defaults to samples
#endif
	sclmode(xss_unitsinbuf),
	curmin(0),curmax(1<<30)
{}
	
xsample::~xsample()
{
//	m_enable(false); // switch off DSP

	if(buf) delete buf; 
}


BL xsample::bufchk() 
{ 
	if(buf->Valid()) {
		if(buf->Update()) {
//			post("%s - buffer updated",thisName()); 
			m_refresh(); 
		}
		return true; 
	}
	else
		return false; 
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
	if(!bufchk()) return; // if invalid do nothing (actually, it should be delayed)

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
	if(!bufchk()) return; // if invalid do nothing (actually, it should be delayed)

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
	if(!bufchk()) return; // if invalid do nothing (actually, it should be delayed)

	mn /= s2u;  // conversion to samples
	if(mn < 0) mn = 0;
	else if(mn > curmax) mn = (F)curmax;
	curmin = (I)(mn+.5);
//	curlen = curmax-curmin;

	m_sclmode();
}

V xsample::m_max(F mx)
{
	if(!bufchk()) return; // if invalid do nothing (actually, it should be delayed)

	mx /= s2u;  // conversion to samples
	if(mx > buf->Frames()) mx = (F)buf->Frames();
	else if(mx < curmin) mx = (F)curmin;
	curmax = (I)(mx+.5);
//	curlen = curmax-curmin;

	m_sclmode();
}

V xsample::m_all()
{
	if(!bufchk()) return; // if invalid do nothing (actually, it should be delayed)

//	curlen = (curmax = buf->Frames())-(curmin = 0);
	curmin = 0; curmax = buf->Frames();
	m_sclmode();
}

V xsample::m_dsp(I /*n*/,S *const * /*insigs*/,S *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

	if(!m_refresh()) s_dsp();
}


V xsample::arrscale(I n,const S *src,S *dst,S add,S mul)
{
	int n8 = n>>3;
	n -= n8<<3;
	while(n8--) {
		dst[0] = (src[0]+add)*mul; 
		dst[1] = (src[1]+add)*mul; 
		dst[2] = (src[2]+add)*mul; 
		dst[3] = (src[3]+add)*mul; 
		dst[4] = (src[4]+add)*mul; 
		dst[5] = (src[5]+add)*mul; 
		dst[6] = (src[6]+add)*mul; 
		dst[7] = (src[7]+add)*mul; 
		src += 8,dst += 8;
	}
	
	while(n--) *(dst++) = (*(src++)+add)*mul; 
}

V xsample::arrmul(I n,const S *src,S *dst,S mul)
{
	int n8 = n>>3;
	n -= n8<<3;
	while(n8--) {
		dst[0] = src[0]*mul; 
		dst[1] = src[1]*mul; 
		dst[2] = src[2]*mul; 
		dst[3] = src[3]*mul; 
		dst[4] = src[4]*mul; 
		dst[5] = src[5]*mul; 
		dst[6] = src[6]*mul; 
		dst[7] = src[7]*mul; 
		src += 8,dst += 8;
	}
	
	while(n--) *(dst++) = *(src++)*mul; 
}




