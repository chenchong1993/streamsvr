//---------------------------------------------------------------------------
#ifndef svrmainH
#define svrmainH
//---------------------------------------------------------------------------
#include "src/rtklib.h"
#include <iostream>
//---------------------------------------------------------------------------
class Streamsvr
{
private:
    string IniFile;
    string Paths[4][4],Cmds[2],CmdsTcp[2];
    string TcpHistory[MAXHIST],TcpMntpHist[MAXHIST];
    string StaPosFile,ExeDirectory,LocalDirectory,SwapInterval;
    string ProxyAddress;
    string ConvMsg[3],ConvOpt[3],AntType,RcvType;
    int ConvEna[3],ConvInp[3],ConvOut[3],StaId,StaSel;
    int TraceLevel,SvrOpt[6],CmdEna[2],CmdEnaTcp[2],NmeaReq,FileSwapMargin;
    double AntPos[3],AntOff[3];
    gtime_t StartTime,EndTime;
    QTimer Timer1,Timer2;

    void SerialOpt(int index, int opt);
    void TcpOpt(int index, int opt);
    void FileOpt(int index, int opt);
    void FtpOpt(int index, int opt);
    void ShowMsg(const QString &msg);
    void SvrStart(void);
    void SvrStop(void);
    void UpdateEnable(void);
    void SetTrayIcon(int index);
    void LoadOpt(void);
    void SaveOpt(void);
public:
    explicit Streamsvr();
    explicit ~Streamsvr();
};
//---------------------------------------------------------------------------
#endif
