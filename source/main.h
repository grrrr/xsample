#ifndef __XSAMPLE_H
#define __XSAMPLE_H

#define VERSION "0.21"


#include <flext.h>

#ifdef PD
extern "C" {
FLEXT_EXT V xrecord_tilde_setup();
FLEXT_EXT V xplay_tilde_setup();
FLEXT_EXT V xgroove_tilde_setup();
}
#endif


#if !defined(_MSC_VER) && !defined(__GNUC__)
// MS VC 6.0 can't handle <int,int> templates?!
// GNUC 2.95.2 dies at compile
#define TMPLOPT
#endif

#ifndef MIN
#define MIN(x,y) ((x) < (y)?(x):(y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y)?(x):(y))
#endif


class xs_obj:
	public flext_dsp
{
	FLEXT_HEADER(xs_obj,flext_dsp)
	
public:
	xs_obj();
	
	enum xs_unit {
		xsu__ = -1,  // don't change
		xsu_sample = 0,xsu_buffer,xsu_ms,xsu_s
	};
	
	enum xs_intp {
		xsi__ = -1,  // don't change
		xsi_none = 0,xsi_4p,xsi_lin
	};
	
	enum xs_sclmd {
		xss__ = -1,  // don't change
		xss_unitsinbuf = 0,xss_unitsinloop,xss_buffer,xss_loop
	};
	
protected:
	buffer *buf;

  	virtual I m_set(I argc,t_atom *argv);
	virtual V m_print() = 0;
	virtual V m_refresh();
	virtual V m_reset();

	virtual V m_units(xs_unit u = xsu__);
	virtual V m_interp(xs_intp u = xsi__);
	virtual V m_sclmode(xs_sclmd u = xss__);

	virtual V m_min(F mn);
	virtual V m_max(F mx);

	xs_unit unitmode;
	xs_intp interp;
	xs_sclmd sclmode;

	I curmin,curmax,curlen;  // in samples
	I sclmin; // in samples
	F sclmul;
	F s2u;  // sample to unit conversion factor

	inline F scale(F smp) const { return (smp-sclmin)*sclmul; }

private:
	static V cb_set(V *c,t_symbol *s,I argc,t_atom *argv);
	static V cb_print(V *c);
	static V cb_refresh(V *c);
	static V cb_reset(V *c);

	static V cb_units(V *c,FI md);
	static V cb_interp(V *c,FI md);
	static V cb_sclmode(V *c,FI md);
};


#endif


