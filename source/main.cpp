#include "main.h"

#ifdef __cplusplus
extern "C" { 
#endif

#ifdef PD
EXT_EXTERN void xsample_setup()
{
	post("xsample objects, (C)2001,2002 Thomas Grill");
	post("xsample: xrecord~, xplay~, xgroove~");
	post("\tsend objects a 'help' message to get assistance");
	post("");

	xrecord_tilde_setup();
	xplay_tilde_setup();
	xgroove_tilde_setup();
}
#endif

#ifdef __cplusplus
}
#endif



V xs_obj::cb_setup(t_class *c)
{
	add_methodG(c,cb_set,"set");
	add_method0(c,cb_print,"print");
	
	add_method1(c,cb_units,"units",A_FLINT);
	add_method1(c,cb_sclmode,"sclmode",A_FLINT);
}

V xs_obj::cb_set(V *c,t_symbol *s,I argc,t_atom *argv) { thisClass(c)->m_set(s,argc,argv); }
V xs_obj::cb_print(V *c) { thisClass(c)->m_print(); }	
V xs_obj::cb_units(V *c,I u) { thisClass(c)->m_units((xs_unit)u); }
V xs_obj::cb_sclmode(V *c,I u) { thisClass(c)->m_sclmode((xs_sclmd)u); }

