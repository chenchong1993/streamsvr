#include <iostream>
#include <stdio.h>
#include <iomanip>
#include <sys/time.h>
#include "Strsvr.h"
using namespace std;

int main()
{
/*    rtkStr rtkS;
	rtkS.showEvent();
	rtkS.SvrStart();*/

	Strsvr strsvr;
	strsvr.SvrStart();
	while (true)
	{
/*		rtkS.TimerTimer();
		if(rtksvr.rtcm[0].obs.n>0)
		{

        }*/

		sleepms(1000);
	}


    return 0;
}


