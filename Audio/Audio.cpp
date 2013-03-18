// Audio.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "mmRoutines.h"
#include "mfRoutines.h"

int _tmain(int argc, _TCHAR* argv[])
{
	if(argc > 1) {
		if(!_stricmp(argv[1], _T("-mm"))) {
			printAudioInfo();
		} else if(!_stricmp(argv[1], _T("-mf"))) {
			initializeMfCom();
			printMfAudioInfo();
			shutdownMfCom();
		} else {
			printf("Invalid option %s\n", argv[1]);
		}
	} else {
		initializeMfCom();
		printMfAudioInfo();
		shutdownMfCom();
	}
	printf("All Done\n");
	return 0;
}

