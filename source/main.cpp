#include "main.h"

#ifdef __cplusplus
extern "C" { 
#endif

#ifdef PD
PD_EXTERN void xsample_setup()
#else
void main()
#endif
{
	post("xsample objects, (C)2001,2002 Thomas Grill");
	post("xsample: xrecord~, xplay~, xgroove~");
	post("");

	xrecord_tilde_setup();
	xplay_tilde_setup();
	xgroove_tilde_setup();
}
#ifdef __cplusplus
}
#endif
