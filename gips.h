#ifndef GIPS_H_INCLUDED
#define GIPS_H_INCLUDED

#include <iostream>
#include "src/rtklib.h"
#include <sstream>

rtksvr_t rtksvr;                        // rtk server struct
stream_t monistr;                       // monitor stream
solopt_t solopt[2];
#define MAXMAPPNT	10
#define TRACEFILE   "rtknavi_%Y%m%d%h%M.trace" // debug trace file

#define KACYCLE     1000                // keep alive cycle (ms)
#define TIMEOUT     10000               // inactive timeout time (ms)
#define DEFAULTPORT 52001               // default monitor port number
#define MAXPORTOFF  9                   // max port number offset
#define MAXTRKSCALE 23                  // track scale

#define SQRT(x)     ((x)<0.0?0.0:sqrt(x))

class rtkStr
{
public:

	explicit rtkStr() {
		//构造

		SvrCycle = SvrBuffSize = 0;
		SolBuffSize = 1000;
		for (int i = 0; i<MAXSTRRTK; i++) {
			StreamC[i] = Stream[i] = Format[i] = 0;
		}
		for (int i = 0; i<3; i++) {
			CmdEna[i][0] = CmdEna[i][1] = 0;
		}

		TimeSys = SolType = PlotType1 = PlotType2 = FreqType1 = FreqType2 = 0;
		TrkType1 = TrkType2 = 0;
		TrkScale1 = TrkScale2 = 5;
		BLMode1 = BLMode2 = 0;
		PSol = PSolS = PSolE = Nsat[0] = Nsat[1] = 0;
		NMapPnt = 0;
		OpenPort = 52001;
		Time = NULL;
		SolStat = Nvsat = NULL;
		SolCurrentStat = 0;
		SolRov = SolRef = Qr = VelRov = Age = Ratio = NULL;

		for (int i = 0; i<2; i++) for (int j = 0; j<MAXSAT; j++) {
			Sat[i][j] = Vsat[i][j] = 0;
			Az[i][j] = El[i][j] = 0.0;
			for (int k = 0; k<NFREQ; k++) Snr[i][j][k] = 0;
		}
		PrcOpt = prcopt_default;
		//SolOpt = solopt_default;

		rtksvrinit(&rtksvr);
		strinit(&monistr);
		rtksoloptinit(&solopt[0]);
		rtksoloptinit(&solopt[2]);

	};
	~rtkStr() {
		//析构
		/*delete[] Time; */  delete[] SolStat; delete[] Nvsat;  delete[] SolRov;
		delete[] SolRef; delete[] Qr;      delete[] VelRov; delete[] Age;
		delete[] Ratio;

	};

public:

	std::string IniFile;
	std::string SolS;
	std::string SolQ;
	//
	int PanelStack, PanelMode;
	int SvrCycle, SvrBuffSize, Scale, SolBuffSize, NavSelect, SavedSol;
	int NmeaReq, NmeaCycle, InTimeTag, OutTimeTag, OutAppend, LogTimeTag, LogAppend;
	int TimeoutTime, ReconTime, SbasCorr, DgpsCorr, TideCorr, FileSwapMargin;
	int Stream[MAXSTRRTK], StreamC[MAXSTRRTK], Format[MAXSTRRTK];
	int CmdEna[3][2], CmdEnaTcp[3][2];
	int TimeSys, SolType, PlotType1, FreqType1, PlotType2, FreqType2;
	int TrkType1, TrkType2, TrkScale1, TrkScale2, BLMode1, BLMode2;
	int MoniPort, OpenPort;

	int PSol, PSolS, PSolE, Nsat[2], SolCurrentStat;
	int Sat[2][MAXSAT], Snr[2][MAXSAT][NFREQ], Vsat[2][MAXSAT];
	double Az[2][MAXSAT], El[2][MAXSAT];
	gtime_t *Time;
	int *SolStat, *Nvsat;
	double *SolRov, *SolRef, *Qr, *VelRov, *Age, *Ratio;
	double TrkOri[3];

	std::string Paths[MAXSTRRTK][4], Cmds[3][2], CmdsTcp[3][2];
	std::string RcvOpt[3], ProxyAddr;
	std::string OutSwapInterval, LogSwapInterval;

	prcopt_t PrcOpt;

	int DebugTraceF, DebugStatusF, OutputGeoidF, BaselineC;
	int RovPosTypeF, RefPosTypeF, RovAntPcvF, RefAntPcvF;
	double NmeaPos[2];
	double Baseline[2];

	std::string History[10], MntpHist[10];

	//QTimer Timer;

	std::string GeoidDataFileF, StaPosFileF, DCBFileF, EOPFileF, TLEFileF;
	std::string TLESatFileF, LocalDirectory, PntName[MAXMAPPNT];

	double PntPos[MAXMAPPNT][3];
	int NMapPnt;

	std::string MarkerName, MarkerComment;

	//第一次显示窗口前触发
	void showEvent() {

		InitSolBuff();
		strinitcom();


	};

	void InitSolBuff() {
		// TODO: Add your implementation code here.
		double ep[] = { 2000,1,1,0,0,0 };
		int i, j;

		trace(3, "InitSolBuff\n");

		delete[] Time;   delete[] SolStat; delete[] Nvsat;  delete[] SolRov;
		delete[] SolRef; delete[] Qr;      delete[] VelRov; delete[] Age;
		delete[] Ratio;

		if (SolBuffSize <= 0) SolBuffSize = 1;
		Time = new gtime_t[SolBuffSize];
		SolStat = new int[SolBuffSize];
		Nvsat = new int[SolBuffSize];
		SolRov = new double[SolBuffSize * 3];
		SolRef = new double[SolBuffSize * 3];
		VelRov = new double[SolBuffSize * 3];
		Qr = new double[SolBuffSize * 9];
		Age = new double[SolBuffSize];
		Ratio = new double[SolBuffSize];
		PSol = PSolS = PSolE = 0;
		for (i = 0; i<SolBuffSize; i++) {
			Time[i] = epoch2time(ep);
			SolStat[i] = Nvsat[i] = 0;
			for (j = 0; j<3; j++) SolRov[j + i * 3] = SolRef[j + i * 3] = VelRov[j + i * 3] = 0.0;
			for (j = 0; j<9; j++) Qr[j + i * 9] = 0.0;
			Age[i] = Ratio[i] = 0.0;
		}
		//
	};//在showeEvent中调用
	//
	void SvrStart() {
		// TODO: Add your implementation code here.
		double pos[3], nmeapos[3];
		/*                                                         TCP客户端   TCP服务端       ntrip客户端     文件          ftp协议   http协议*/
		int itype[] = { STR_SERIAL,STR_TCPCLI,STR_TCPSVR,STR_NTRIPCLI,STR_FILE,STR_FTP,STR_HTTP };
		int otype[] = { STR_SERIAL,STR_TCPCLI,STR_TCPSVR,STR_NTRIPSVR,STR_FILE };
		int strs[MAXSTRRTK] = { 0 }/*,stropt[8] = { 0 }*/;
		char *paths[8]={ 0 }, *cmds[3] = { 0 }, *rcvopts[3] = { 0 };
        char *cmds_periodic[3] = { 0 };
        char msg[128] = "";
        for (int i = 0;i < 8;i++){
            if(i == 0){
                    strs[i] = 4;
                    Format[i] = 1;
            }else{
            strs[i] = 0;
            Format[i] = 0;
            }
        }

		cmds_periodic[0] = cmds_periodic[1] = cmds_periodic[2] = "";
        SvrBuffSize = 32768;
        SvrCycle = 10;
        NavSelect = 0;
        NmeaCycle = 5000;
        NmeaCycle = NmeaCycle<1000 ? 1000 : NmeaCycle;
        NmeaReq = 0;
        /*初始化prcopt结构体*/
        rtkprcoptinit(&PrcOpt,&rtksvr);

        /*第一路数据流赋值*/
		StreamC[0] = 1; Stream[0] = 1; /*Format[0] = 1;*/
        Paths[0][0] = "";
		Paths[0][1] = ":@124.205.216.26:35946/::";
		Paths[0][2] = "::x1";
		Paths[0][3] = "";
        /*第二路数据流赋值*/
		StreamC[1] = 1; Stream[1] = 1; /*Format[1] = 1;*/
/*		Paths[1][0] = "";
		Paths[1][1] = ":@192.189.1.20:12020/::";
		Paths[1][2] = "::x1";
		Paths[1][3] = "";*/

		DebugTraceF = 5;//0

		TimeoutTime = 10000;
		ReconTime = 10000;

		FileSwapMargin = 30;

		//peizhi
		SolType = 2;//屏幕打印格式

		/*第三路第四路*/
		//OutputStr
		StreamC[3] = 1;
		StreamC[4] = 0;
		Stream[3] = 4;
		Stream[4] = 0;
/*		Format[3] = 1;//1：XYZ,2:ENU
		Format[4] = 0;*/
		Paths[3][2] = "res.pos";
		Paths[4][2] = "";
		//SwapInterval = SwapIntv->currentText();
		//outPutFlag();//启动输出

		//
		trace(3, "SvrStart\n");




/*		for (int i = 0; i<3; i++)
			strs[i] = StreamC[i] ? itype[Stream[i]] : STR_NONE;
		for (int i = 3; i<5; i++)
			strs[i] = StreamC[i] ? otype[Stream[i]] : STR_NONE;
		for (int i = 5; i<8; i++)
			strs[i] = StreamC[i] ? otype[Stream[i]] : STR_NONE;*/

       for (int i = 0; i<8; i++) {
			paths[i] = new char[1024];
			paths[i][0] = '\0';
			//不同数据流协议配置文件定义
			if (strs[i] == STR_NONE) strcpy(paths[i], "");
			else if (strs[i] == STR_SERIAL) strcpy(paths[i], Paths[i][0].c_str());
			else if (strs[i] == STR_FILE) strcpy(paths[i], Paths[i][2].c_str());
			else if (strs[i] == STR_FTP || strs[i] == STR_HTTP) strcpy(paths[i], Paths[i][3].c_str());
			else strcpy(paths[i], Paths[i][1].c_str());
		}
		for (int i = 0; i<3; i++) {
			cmds[i] = new char[1024];
			rcvopts[i] = new char[1024];
			cmds[i][0] = rcvopts[i][0] = '\0';
			if (strs[i] == STR_SERIAL) {
				if (CmdEna[i][0]) strcpy(cmds[i], Cmds[i][0].c_str());
			}
			else if (strs[i] == STR_TCPCLI || strs[i] == STR_TCPSVR ||
				strs[i] == STR_NTRIPCLI) {
				if (CmdEnaTcp[i][0]) strcpy(cmds[i], CmdsTcp[i][0].c_str());
			}
			strcpy(rcvopts[i], RcvOpt[i].c_str());
		}

		pos[0] = NmeaPos[0] * D2R;
		pos[1] = NmeaPos[1] * D2R;
		pos[2] = 0.0;
		pos2ecef(pos, nmeapos);

		strsetdir(LocalDirectory.c_str());
		strsetproxy(ProxyAddr.c_str());

		//for (i = 3; i<8; i++) {//文件
		//	if (strs[i] == STR_FILE && !ConfOverwrite(paths[i])) return;
		//}
		if (DebugTraceF>0) {
			traceopen(TRACEFILE);
			tracelevel(DebugTraceF);
		}
		/*if (DebugStatusF>0) {
		rtkopenstat(STATFILE, DebugStatusF);
		}*/
		if (solopt[0].geoid>0 && GeoidDataFileF != "") {
			opengeoid(solopt[0].geoid, GeoidDataFileF.c_str());
		}
		if (DCBFileF != "") {
			readdcb(DCBFileF.c_str(), &rtksvr.nav, NULL);
		}


/*		stropt[0] = TimeoutTime;
		stropt[1] = ReconTime;
		stropt[2] = 1000;
		stropt[3] = SvrBuffSize;
		stropt[4] = FileSwapMargin;
		strsetopt(stropt);*/

		// start rtk server


		for(int i=0;i<8;i++){
            printf("%s\n",paths[i]);
		}
		if (!rtksvrstart(&rtksvr /*rtklib自带初始化函数*/, SvrCycle, SvrBuffSize, strs, paths, Format, NavSelect,
			cmds, cmds_periodic, rcvopts, NmeaCycle, NmeaReq, nmeapos, &PrcOpt/*自己构造了初始化函数*/, solopt/*自己构造了初始化函数*/,
			&monistr/*rtklib自带初始化函数*/, msg)) {
			traceclose();
			for (int i = 0; i<8; i++) delete[] paths[i];
			for (int i = 0; i<3; i++) delete[] rcvopts[i];
			for (int i = 0; i<3; i++) delete[] cmds[i];
			return;
		}
		//


		//
		for (int i = 0; i<8; i++) delete[] paths[i];
		for (int i = 0; i<3; i++) delete[] rcvopts[i];
		for (int i = 0; i<3; i++) delete[] cmds[i];
		PSol = PSolS = PSolE = 0;
		SolStat[0] = Nvsat[0] = 0;
		for (int i = 0; i<3; i++) SolRov[i] = SolRef[i] = VelRov[i] = 0.0;
		for (int i = 0; i<9; i++) Qr[i] = 0.0;
		Age[0] = Ratio[0] = 0.0;
		Nsat[0] = Nsat[1] = 0;

		UpdateTime();
		UpdatePos();
	};

	// strop rtk server ---------------------------------------------------------
	void  SvrStop(void)
	{
		char *cmds[3] = { 0 };
		int i, n, m, str;

		trace(3, "SvrStop\n");

		for (i = 0; i<3; i++) {
			cmds[i] = new char[1024];
			cmds[i][0] = '\0';
			str = rtksvr.stream[i].type;

			if (str == STR_SERIAL) {
				if (CmdEna[i][1]) strcpy(cmds[i], Cmds[i][1].c_str());
			}
			else if (str == STR_TCPCLI || str == STR_TCPSVR || str == STR_NTRIPCLI) {
				if (CmdEnaTcp[i][1]) strcpy(cmds[i], CmdsTcp[i][1].c_str());
			}
		}
		rtksvrstop(&rtksvr, cmds);
		for (i = 0; i<3; i++) delete[] cmds[i];

		n = PSolE - PSolS; if (n<0) n += SolBuffSize;
		m = PSol - PSolS;  if (m<0) m += SolBuffSize;
		if (n>0) {
			std::cout << "ScbSol->setMaximum(n - 1); ScbSol->setValue(m)" << std::endl;;
		}
		//Message->setText("");

		if (DebugTraceF>0) traceclose();
		if (DebugStatusF>0) rtkclosestat();
		if (solopt[0].geoid>0 && GeoidDataFileF != "") closegeoid();
	}
	//
	void UpdatePos() {
		// TODO: Add your implementation code here.
		//QLabel *label[] = { Plabel1,Plabel2,Plabel3,Pos1,Pos2,Pos3,LabelStd,LabelNSat };
		//QString sol[] = { tr("----"),tr("FIX"),tr("FLOAT"),tr("SBAS"),tr("DGPS"),tr("SINGLE"),tr("PPP") };
		std::string label[8];
		std::string sol[] = { "----; ", "FIX; ","FLOAT; ","SBAS; ","DPGS; ","SINGLE; ","PPP; " };
		std::string s[9], ext = "";
		std::ostringstream stream;
		//QString color[] = { "silver","green","rgb(0,170,255)","rgb(255,0,255)","blue","red","rgb(128,0,128)" };
		double *rr = SolRov + PSol * 3, *rb = SolRef + PSol * 3, *qr = Qr + PSol * 9, pos[3] = { 0 }, Qe[9] = { 0 };
		double dms1[3] = { 0 }, dms2[3] = { 0 }, bl[3] = { 0 }, enu[3] = { 0 }, pitch = 0.0, yaw = 0.0, len;
		int i, stat = SolStat[PSol];

		trace(4, "UpdatePos\n");

		if (rtksvr.rtk.opt.mode == PMODE_STATIC || rtksvr.rtk.opt.mode == PMODE_PPP_STATIC) {
			ext = " (S)";
		}
		else if (rtksvr.rtk.opt.mode == PMODE_FIXED || rtksvr.rtk.opt.mode == PMODE_PPP_FIXED) {
			ext = " (F)";
		}
		label[0] = "Solution" + ext + ":";

		label[0] += sol[stat];

		if (norm(rr, 3)>0.0&&norm(rb, 3)>0.0) {
			for (i = 0; i<3; i++) bl[i] = rr[i] - rb[i];
		}
		len = norm(bl, 3);

	};
	void degtodms(double deg, double *dms) {
		double sgn = 1.0;
		if (deg<0.0) { deg = -deg; sgn = -1.0; }
		dms[0] = floor(deg);
		dms[1] = floor((deg - dms[0])*60.0);
		dms[2] = (deg - dms[0] - dms[1] / 60.0) * 3600;
		dms[0] *= sgn;
	};

	void TimerTimer() {
		// TODO: Add your implementation code here.
		static int n = 0, inactive = 0;
		sol_t *sol;
		int i, update = 0;

		trace(4, "TimerTimer\n");

		rtksvrlock(&rtksvr);

		for (i = 0; i<rtksvr.nsol; i++) {
			sol = rtksvr.solbuf + i;
			UpdateLog(sol->stat, sol->time, sol->rr, sol->qr, rtksvr.rtk.rb, sol->ns,
				sol->age, sol->ratio);
			update = 1;
		}
		rtksvr.nsol = 0;
		SolCurrentStat = rtksvr.state ? rtksvr.rtk.sol.stat : 0;

		rtksvrunlock(&rtksvr);

		if (update) {
			UpdateTime();
			UpdatePos();
			inactive = 0;
		}
		else {
			if (++inactive * 100.0 >TIMEOUT) SolCurrentStat = 0;
		}
		if (SolCurrentStat) {
			//Svr->setStyleSheet("QLabel {background-color: rgb(0,255,0);}");
			//LabelTime->setStyleSheet("QLabel { color: black;}");
			std::cout << "SolCuttentStat != 0" << std::endl;
		}
		else {
			//IndSol->setStyleSheet("QLabel {color: white; background-color: white;}");
			//Solution->setStyleSheet("QLabel {color: gray;}");
			//Svr->setStyleSheet(rtksvr.state ? "QLabel {background-color: green; }" : "QLabel {background-color: gray; }");
			std::cout << "SolCuttentStat = 0" << std::endl;
		}
		//if (!(++n % 5)) UpdatePlot();//update solution plot

		UpdateStr();

		// keep alive for monitor port
		if (!(++n % (KACYCLE / 100)) && OpenPort) {
			unsigned char buf[1];
			buf[0] = '\r';
			strwrite(&monistr, buf, 1);
		}
	};
	void UpdateLog(int stat, gtime_t time, double *rr,
		float *qr, double *rb, int ns, double age, double ratio) {
		// TODO: Add your implementation code here.
		int i;

		if (!stat) return;

		trace(4, "UpdateLog\n");

		SolStat[PSolE] = stat; Time[PSolE] = time; Nvsat[PSolE] = ns; Age[PSolE] = age;
		Ratio[PSolE] = ratio;
		for (i = 0; i<3; i++) {
			SolRov[i + PSolE * 3] = rr[i];
			SolRef[i + PSolE * 3] = rb[i];
			VelRov[i + PSolE * 3] = rr[i + 3];
		}
		Qr[PSolE * 9] = qr[0];
		Qr[4 + PSolE * 9] = qr[1];
		Qr[8 + PSolE * 9] = qr[2];
		Qr[1 + PSolE * 9] = Qr[3 + PSolE * 9] = qr[3];
		Qr[5 + PSolE * 9] = Qr[7 + PSolE * 9] = qr[4];
		Qr[2 + PSolE * 9] = Qr[6 + PSolE * 9] = qr[5];

		PSol = PSolE;
		if (++PSolE >= SolBuffSize) PSolE = 0;
		if (PSolE == PSolS && ++PSolS >= SolBuffSize) PSolS = 0;
	};
	void UpdateTime() {
		// TODO: Add your implementation code here.
		gtime_t time = Time[PSol];
		struct tm *t;
		double tow;
		int week;
		char tstr[64];
		std::string str;
		std::ostringstream stream;

		trace(4, "UpdateTime\n");

		if (TimeSys == 0) time2str(time, tstr, 1);
		else if (TimeSys == 1) time2str(gpst2utc(time), tstr, 1);
		else if (TimeSys == 2) {
			time = gpst2utc(time);
			if (!(t = localtime(&time.time))) str = "2000/01/01 00:00:00.0";
			else
			{
				//str = QString("%1/%2/%3 %4:%5:%6.%7").arg(t->tm_year + 1900, 4, 10, QChar('0'))
				//	.arg(t->tm_mon + 1, 2, 10, QChar('0')).arg(t->tm_mday, 2, 10, QChar('0')).arg(t->tm_hour, 2, 10, QChar('0')).arg(t->tm_min, 2, 10, QChar('0'))
				//	.arg(t->tm_sec, 2, 10, QChar('0')).arg(static_cast<int>(time.sec * 10));
				stream << t->tm_year + 1900 << "/" << t->tm_mon + 1 << "/" << t->tm_mday << "/"
					<< t->tm_hour << "/" << t->tm_min << "/" << t->tm_sec << "." << static_cast<int>(time.sec * 10);
				str = stream.str(); stream.clear();
			}
		}
		else if (TimeSys == 3) {
			tow = time2gpst(time, &week);
			//str = QString("week %1 %2 s").arg(week, 4, 10, QChar('0')).arg(tow, 8, 'f', 1);
			stream << "week " << week << " " << tow << " s";
			str = stream.str(); stream.clear();
		}

//		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED|FOREGROUND_GREEN);
//		std::cout << str.c_str() << std::endl;
//		std::cout << tstr << std::endl;
//		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY |
//		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	};
	void UpdateStr() {
		// TODO: Add your implementation code here.
		//QString color[] = { "red","gray","orange","rgb(0,128,0)","rgb(0,255,0)" };

		//QLabel *ind[MAXSTRRTK] = { Str1,Str2,Str3,Str4,Str5,Str6,Str7,Str8 };
		int i, sstat[MAXSTRRTK] = { 0 };
		char msg[MAXSTRMSG] = "";

		trace(4, "UpdateStr\n");

		rtksvrsstat(&rtksvr, sstat, msg);
		/*for (i = 0; i<MAXSTRRTK; i++) {
		ind[i]->setStyleSheet(QString("QLabel {background-color: %1}").arg(color[sstat[i] + 1]));
		if (sstat[i]) {
		Message->setText(msg);
		}
		}*/
		std::cout << msg << std::endl;
	};
	//void outPutFlag();
};



#endif // GIPS_H_INCLUDED
