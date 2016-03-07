#pragma once
#include "stdafx.h"

#define BUFSIZE 4096

void init_child_process();
int WriteToPipeFromSocket(SOCKET &socket);
int WriteToSocketFromPipe(SOCKET &socket);
