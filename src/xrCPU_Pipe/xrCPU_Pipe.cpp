#include "stdafx.h"
#pragma hdrstop

BOOL WINAPI DllMain ( HINSTANCE hinstDLL , DWORD fdwReason , LPVOID lpvReserved )
{
	return TRUE;
}

extern xrSkin1W			xrSkin1W_x86;
extern xrSkin2W			xrSkin2W_x86;
extern xrSkin3W			xrSkin3W_x86;
extern xrSkin4W			xrSkin4W_x86;

xrSkin4W* skin4W_func = NULL;

extern xrPLC_calc3		PLC_calc3_x86;
extern xrPLC_calc3		PLC_calc3_SSE;


extern "C" {
	__declspec(dllexport) void	__cdecl	xrBind_PSGP	( xrDispatchTable* T , _processor_info* ID )
	{
		// generic
		T->skin1W	= xrSkin1W_x86;
		T->skin2W	= xrSkin2W_x86;
		T->skin3W	= xrSkin3W_x86;
		T->skin4W	= xrSkin4W_x86;
		skin4W_func = xrSkin4W_x86;
		T->PLC_calc3 = PLC_calc3_x86;
	
		// SSE
		if (ID->feature & _CPU_FEATURE_SSE) {
			T->PLC_calc3 = PLC_calc3_SSE;
		}
	}
};
