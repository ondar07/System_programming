// client_server_with_child_process.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include "redirected_input_ouput.h"
int __cdecl cli(int argc, char **argv);
int __cdecl srv(void);

int main(int argc, TCHAR *argv[])
{

	if ((argc == 2) && (!strcmp(argv[1], "cli"))) {
		return cli(argc, argv);
	}
	if ((argc == 2) && (!strcmp(argv[1], "srv"))) {
		return srv();
	}

    return 0;

}

