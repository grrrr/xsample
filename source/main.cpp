/*
xsample - extended sample objects for Max/MSP and pd (pure data)

Copyright (c) 2001-2005 Thomas Grill (gr@grrrr.org)
For information on usage and redistribution, and for a DISCLAIMER OF ALL
WARRANTIES, see the file, "license.txt," in this distribution.  
*/

#include "main.h"


// Initialization function for xsample library
static void xsample_main()
{
	flext::post("-------------------------------");
	flext::post("xsample objects, version " XSAMPLE_VERSION);
    flext::post("");
	flext::post("  xrecord~, xplay~, xgroove~   ");
    flext::post("  (C)2001-2005 Thomas Grill    ");
#ifdef FLEXT_DEBUG
    flext::post("");
    flext::post("DEBUG BUILD - " __DATE__ " " __TIME__);
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
	curmin(0),curmax(1<<(sizeof(curmax)*8-2))
{}
	
xsample::~xsample() {}

bool xsample::Finalize()
{
    if(!flext_dsp::Finalize()) return false;

    // flags have been set in constructor
    Refresh();
    return true;
}

int xsample::ChkBuffer(bool refresh) 
{      
    if(!buf.Ok()) return 0;
    
    if(buf.Update()) {
#ifdef FLEXT_DEBUG
        post("%s - buffer update!",thisName());
#endif
        Update(xsc_buffer);
        if(refresh) { 
            Refresh();
            return buf.Ok() && buf.Valid()?1:0;
        }
        else 
            return buf.Valid()?1:0;
    }
    else
        return buf.Valid()?-1:0;
}

/* called after all buffer objects have been created in the patch */
void xsample::m_loadbang() 
{
    ChkBuffer(true);
}

void xsample::m_set(int argc,const t_atom *argv)
{
	const t_symbol *sym = argc >= 1?GetASymbol(argv[0]):NULL;
	int r = buf.Set(sym);
	if(sym && r < 0) 
        post("%s - can't find buffer %s",thisName(),GetString(sym));
    Update(xsc_buffer,true);
}

void xsample::m_min(float mn)
{
    int ret = ChkBuffer(true);

	if(ret && s2u) {
		long cmn = CASTINT<long>(mn/s2u+0.5f);  // conversion to samples

		if(cmn < 0) 
            curmin = 0;
		else if(cmn > curmax) 
            curmin = curmax;
		else
            curmin = cmn;

        Update(xsc_range,true);
	}
}

void xsample::m_max(float mx)
{
    int ret = ChkBuffer(true);

	if(ret && s2u) {
		long cmx = CASTINT<long>(mx/s2u+0.5f);  // conversion to samples

		if(cmx > buf.Frames()) 
            curmax = buf.Frames();
		else if(cmx < curmin) 
            curmax = curmin;
        else
		    curmax = cmx;

        Update(xsc_range,true);
	}
}

void xsample::m_dsp(int /*n*/,t_sample *const * /*insigs*/,t_sample *const * /*outsigs*/)
{
	// this is hopefully called at change of sample rate ?!

#ifdef FLEXT_DEBUG
        post("%s - DSP reset!",thisName());
#endif

    // for PD at least this is also called if a table has been deleted...
    // then we must reset the buffer

    Update(xsc_srate|xsc_buffer,true);
}

void xsample::DoReset() 
{
    ResetRange();
}

void xsample::DoUpdate(unsigned int flags)
{
    if(flags&xsc_buffer)
        buf.Set();

    if(flags&xsc_range && buf.Ok()) {
        if(curmin < 0) curmin = 0;
        if(curmax > buf.Frames()) curmax = buf.Frames();
    }

    if(flags&xsc_units) {
	    switch(unitmode) {
		    case xsu_sample: // samples
			    s2u = 1;
			    break;
		    case xsu_buffer: // buffer size
			    s2u = buf.Ok() && buf.Frames()?1.f/buf.Frames():0;
			    break;
		    case xsu_ms: // ms
			    s2u = 1000.f/Samplerate();
			    break;
		    case xsu_s: // s
			    s2u = 1.f/Samplerate();
			    break;
		    default:
			    post("%s - Unknown unit mode",thisName());
	    }

	    switch(sclmode) {
		    case xss_unitsinbuf: // samples/units
			    sclmin = 0; sclmul = s2u;
			    break;
		    case xss_unitsinloop: // samples/units from recmin to recmax
			    sclmin = curmin; sclmul = s2u;
			    break;
		    case xss_buffer: // unity between 0 and buffer size
			    sclmin = 0; sclmul = buf.Ok() && buf.Frames()?1.f/buf.Frames():0;
			    break;
		    case xss_loop:	// unity between recmin and recmax
			    sclmin = curmin; sclmul = curmin < curmax?1.f/(curmax-curmin):0;
			    break;
		    default:
			    post("%s - Unknown scale mode",thisName());
	    }
    }
}
