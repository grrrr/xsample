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
	virtual ~xs_obj();
	
	enum xs_unit {
		xsu_sample,xsu_buffer,xsu_ms,xsu_s
	};
	
	enum xs_sclmd {
		xss_unitsinbuf,xss_unitsinloop,xss_buffer,xss_loop
	};
	
protected:
	virtual V m_set(t_symbol *s,I argc,t_atom *argv);
	virtual V m_print() = 0;
	virtual V m_units(xs_unit u) = 0;
	virtual V m_sclmode(xs_sclmd u) = 0;

private:
	static V cb_set(V *c,t_symbol *s,I argc,t_atom *argv);
	static V cb_print(V *c);
	static V cb_units(V *c,I un);
	static V cb_sclmode(V *c,I md);
};




#endif

