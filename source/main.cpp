/*

xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2004 Thomas Grill (xovo@gmx.net)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  

*/

#include "main.h"


// Initialization function for xsample library
static V xsample_main()
{
	flext::post("-------------------------------");
	flext::post("xsample objects, version " XSAMPLE_VERSION);
    flext::post("");
	flext::post("  xrecord~, xplay~, xgroove~   ");
    flext::post("  (C)2001-2004 Thomas Grill    ");
#ifdef FLEXT_DEBUG
    flext::post("          DEBUG BUILD          ");
#endif
	flext::post("-------------------------------");

	// call the objects' setup routines
	FLEXT_DSP_SETUP(xrecord);
	FLEXT_DSP_SETUP(xplay);
	FLEXT_DSP_SETUP(xgroove);
}

// setup the library
FLEXT_LIB_SETUP(xsample,xsample_main)

// ------------------------------

void xsample::setup(t_classid c)
{
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
    update(xsc_all),
#if FLEXT_SYS == FLEXT_SYS_MAX
	unitmode(xsu_ms),	   // Max/MSP defaults to milliseconds
#else
	unitmode(xsu_sample),  // PD defaults to samples
#endif
	sclmode(xss_unitsinbuf),
	curmin(0),curmax(1<<31)
{}
	
xsample::~xsample()
{
//	m_enable(false); // switch off DSP
}

bool xsample::Init()
{
    if(!flext_dsp::Init()) return false;
    DoUpdate();
    return true;
}

bool xsample::ChkBuffer() 
{ 
	if(!buf.Valid()) return false;
        
    if(buf.Update() || buf.Set())) {
        Update(xsc_buffer);
        s_dsp(); // channel count may have changed
        return true;
	}
    return false;
}

/* called after all buffer objects have been created in the patch */
void xsample::m_loadbang() 
{
	m_reset();
}

void xsample::m_set(I argc,const t_atom *argv)
{
	const t_symbol *sym = argc >= 1?GetASymbol(argv[0]):NULL;
	int r = buf->Set(sym);
	if(sym) {
        if(r < 0) post("%s - can't find buffer %s",thisName(),GetString(sym));
        else ChkBuffer();
    }
    DoUpdate();
}

V xsample::m_units(xs_unit mode)
{
    unitmode = mode;
    Update(xsc_units);
    DoUpdate();
}

V xsample::m_sclmode(xs_sclmd mode)
{
    sclmode = mode;
    Update(xsc_sclmd);
    DoUpdate();
}

V xsample::m_min(F mn)
{
    ChkBuffer();
    DoUpdate();

	if(s2u) {
		mn /= s2u;  // conversion to samples
		if(mn < 0) mn = 0;
		else if(mn > curmax) mn = (F)curmax;
		curmin = (I)(mn+.5);

        Update(xsc_range);
        DoUpdate();
	}
}

V xsample::m_max(F mx)
{
    ChkBuffer();
    DoUpdate();

	if(s2u) {
		mx /= s2u;  // conversion to samples
		if(mx > buf.Frames()) mx = (F)buf.Frames();
		else if(mx < curmin) mx = (F)curmin;
		curmax = (I)(mx+.5);

        Update(xsc_range);
        DoUpdate();
	}
}

V xsample::m_all()
{
    ChkBuffer();
    DoUpdate();
    
	curmin = 0; 
    curmax = buf.Frames();

    Update(xsc_range);
    DoUpdate();
}

V xsample::m_dsp(I /*n*/,S *const * /*insigs*/,S *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

    ChkBuffer();
    Update(xsc_srate);
    DoUpdate();
}

void xsample::DoUpdate()
{
    if(update&xsc_units) SetUnits();
    if(update&xsc_sclmd) SetSclmd();

    if(update&xsc_range) SetRange();
    if(update&xsc_play) SetPlay();
    
    update = xsc__;
}

/*! buffer must have been checked beforehand */
void xsample::SetUnits()
{
	switch(unitmode) {
		case xsu_sample: // samples
			s2u = 1;
			break;
		case xsu_buffer: // buffer size
			s2u = bufchk()?1.f/buf.Frames():0;
			break;
		case xsu_ms: // ms
			s2u = 1000.f/Samplerate();
			break;
		case xsu_s: // s
			s2u = 1.f/Samplerate();
			break;
		default:
			throw "Unknown unit mode";
	}
}

/*! buffer must have been checked beforehand 
    units must have been set beforehand
*/
void xsample::SetSclmd()
{
	switch(sclmode) {
		case 0: // samples/units
			sclmin = 0; sclmul = s2u;
			break;
		case 1: // samples/units from recmin to recmax
			sclmin = curmin; sclmul = s2u;
			break;
		case 2: // unity between 0 and buffer size
			sclmin = 0; sclmul = (bufchk() && buf.Frames())?1.f/buf.Frames():0;
			break;
		case 3:	// unity between recmin and recmax
			sclmin = curmin; sclmul = curmin != curmax?1.f/(curmax-curmin):0;
			break;
		default:
			throw "Unknown scale mode";
	}
}

void xsample::SetPlay()
{
}
