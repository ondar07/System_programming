#pragma once
#include "stdafx.h"

#define BUFSIZE 4096

PROCESS_INFORMATION InitChildProcess();
void TerminateChildProcess(PROCESS_INFORMATION procInfo);
int WriteToPipeFromSocket(SOCKET &socket);
int WriteToSocketFromPipe(SOCKET &socket);
