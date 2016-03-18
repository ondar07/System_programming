#pragma once

#include "stdafx.h"

#define SVCNAME TEXT("RemConsSrv")     // a name of the service

VOID SvcInstall(void);
VOID WINAPI SvcMain(DWORD, LPTSTR *);
VOID SvcReportEvent(LPTSTR);
VOID __stdcall SvcStart();
VOID __stdcall SvcRemove();
VOID __stdcall SvcStop();
