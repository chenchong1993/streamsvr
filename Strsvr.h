#ifndef STRSVR_H
#define STRSVR_H
#define MAXHIST 10
#define MAXMAPPNT	10
#define PRGNAME     "STRSVR-QT"        // program name
#define TRACEFILE   "rtknavi_%Y%m%d%h%M.trace" // debug trace file
#define KACYCLE     1000                // keep alive cycle (ms)
#define TIMEOUT     10000               // inactive timeout time (ms)
#define DEFAULTPORT 52001               // default monitor port number
#define MAXPORTOFF  9                   // max port number offset
#define MAXTRKSCALE 23                  // track scale
#define SQRT(x)     ((x)<0.0?0.0:sqrt(x))
#define MIN(x,y)    ((x)<(y)?(x):(y))
#include "src/rtklib.h"
#include <iostream>
#include <sstream>
using namespace std;
class Strsvr
{
    public:
        Strsvr();
        virtual ~Strsvr();

        std::string IniFile;
        std::string Paths[4][4];
        std::string Cmds[2],CmdsTcp[2];
        std::string TcpHistory[MAXHIST],TcpMntpHist[MAXHIST];
        std::string StaPosFile,ExeDirectory,LocalDirectory,SwapInterval;
        std::string ProxyAddress;
        std::string ConvMsg[3],ConvOpt[3],AntType,RcvType;
        int ConvEna[3],ConvInp[3],ConvOut[3],StaId,StaSel;
        int TraceLevel,SvrOpt[6],CmdEna[2],CmdEnaTcp[2],NmeaReq,FileSwapMargin;
        double AntPos[3],AntOff[3];
        /*下面为loadopt函数里用的参数的定义，读配置文件进行初始化*/
        int Input,Output1,Output2,Output3;
       // int TraceLevel,NmeaReq,FileSwapMargin,StaId,StaSel;
        //std::string AntType,RcvType;

        //gtime_t StartTime,EndTime;
        //QTimer Timer1,Timer2;

/*        void SerialOpt(int index, int opt);
        void TcpOpt(int index, int opt);
        void FileOpt(int index, int opt);
        void FtpOpt(int index, int opt);*/
        //void ShowMsg(const QString &msg);
        void SvrStart(void);
        void SvrStop(void);
/*        void UpdateEnable(void);
        void SetTrayIcon(int index);*/
        void LoadOpt(void);
/*        void SaveOpt(void);*/
};

#endif // STRSVR_H
