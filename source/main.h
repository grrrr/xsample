#ifndef __XSAMPLE_H
#define __XSAMPLE_H

#define VERSION "0.2"


#include <_cppext.h>

#ifdef PD
extern "C" {
EXT_EXTERN V xrecord_tilde_setup();
EXT_EXTERN V xplay_tilde_setup();
EXT_EXTERN V xgroove_tilde_setup();
}
#endif


class xs_obj:
	public bufdsp_obj
{
	CPPEXTERN_HEADER(xs_obj,bufdsp_obj)
	
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
	virtual V m_set(t_symbol *s,I argc,t_atom *argv);
	virtual V m_print() = 0;
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
	static V cb_units(V *c,FI md);
	static V cb_interp(V *c,FI md);
	static V cb_sclmode(V *c,FI md);
};


#endif

