#include "Strsvr.h"

strsvr_t strsvr;

Strsvr::Strsvr()
{
    //ctor
    strsvrinit(&strsvr,1);
}

Strsvr::~Strsvr()
{
    //dtor
}
// start stream server ------------------------------------------------------
void Strsvr::SvrStart(void)
{
    LoadOpt();
    strconv_t *conv[3]={0};
    static char str[4][1024];
    int itype[]={
        STR_SERIAL,STR_TCPCLI,STR_TCPSVR,STR_NTRIPCLI,STR_FILE,STR_FTP,STR_HTTP
    };
    int otype[]={
        STR_NONE,STR_SERIAL,STR_TCPCLI,STR_TCPSVR,STR_NTRIPSVR,STR_FILE
    };
    int /*ip[]={0,1,1,1,2,3,3},*/strs[4]={0},opt[7]={0},n;
    char *paths[4],/*filepath[1024],*/buff[1024];
    char cmd[1024];
    char *ant[3]={0},*rcv[3]={0},*p;

    if (TraceLevel>0) {
        traceopen(TRACEFILE);
        tracelevel(TraceLevel);
    }
    for (int i=0;i<4;i++) paths[i]=str[i];

    //刘类型设置，一进三出．
    strs[0]=itype[Input];
    strs[1]=otype[Output1];
    strs[2]=otype[Output2];
    strs[3]=otype[Output3];

    strcpy(paths[0],Paths[0][1].c_str()); //一进三出模式，可以改成一进一出，这里是把输入的ip地址赋值给path
    strcpy(paths[1],Paths[0][1].c_str());
    strcpy(paths[2],Paths[0][1].c_str());
    strcpy(paths[3],Paths[0][1].c_str());
    /*赋值给cmd*/
    //const char * Cmds0_char=(char*)Cmds[0].data();
    //const char * CmdsTcp0_char=(char*)CmdsTcp[0].data();
    if (CmdEna[0]) strncpy(cmd,(const char*)Cmds[0].data(),1024);
    if (CmdEnaTcp[0]) strncpy(cmd,(const char*)CmdsTcp[0].data(),1024);

    for (int i=0;i<5;i++) {
        opt[i]=SvrOpt[i];
    }
    opt[5]=NmeaReq?SvrOpt[5]:0;
    opt[6]=FileSwapMargin;

/*    for (int i=1;i<4;i++) {
        if (strs[i]!=STR_FILE) continue; //当流类型为文件的时候才执行下面的文件。这里的for循环应该是不需要的
        strcpy(filepath,paths[i]);
        if (strstr(filepath,"::A")) continue;
        if ((p=strstr(filepath,"::"))) *p='\0';
        if (!(fp=fopen(filepath,"r"))) continue;
        fclose(fp);
        if (QMessageBox::question(this,tr("Overwrite"),tr("File %1 exists. \nDo you want to overwrite?").arg(filepath))!=QMessageBox::Yes) return;
    }*/
/*    strsetdir(qPrintable(LocalDirectory));
    strsetproxy(qPrintable(ProxyAddress));*/

    for (int i=0;i<3;i++) {
        if (!ConvEna[i]) continue;
        if (!(conv[i]=strconvnew(ConvInp[i],ConvOut[i],(const char *)ConvMsg[i].data(),
                                 StaId,StaSel,(const char *)ConvOpt[i].data()))) continue;
        strcpy(buff,(const char *)AntType.data());
        for (p=strtok(buff,","),n=0;p&&n<3;p=strtok(NULL,",")) ant[n++]=p;
        strcpy(conv[i]->out.sta.antdes,ant[0]);
        strcpy(conv[i]->out.sta.antsno,ant[1]);
        conv[i]->out.sta.antsetup=atoi(ant[2]);
        strcpy(buff,(const char *)RcvType.data());
        for (p=strtok(buff,","),n=0;p&&n<3;p=strtok(NULL,",")) rcv[n++]=p;
        strcpy(conv[i]->out.sta.rectype,rcv[0]);
        strcpy(conv[i]->out.sta.recver ,rcv[1]);
        strcpy(conv[i]->out.sta.recsno ,rcv[2]);
        matcpy(conv[i]->out.sta.pos,AntPos,3,1);
        matcpy(conv[i]->out.sta.del,AntOff,3,1);
    }
    // stream server start
    if (!strsvrstart(&strsvr,opt,strs,paths,conv,(char**)cmd,0,AntPos)) return;
}
// stop stream server -------------------------------------------------------
void Strsvr::SvrStop(void)
{
    char cmd[1024];

    if (CmdEna[1]) strncpy(cmd,(const char *)Cmds[1].data(),1024);
    if (CmdEnaTcp[1]) strncpy(cmd,(const char *)CmdsTcp[1].data(),1024);

    strsvrstop(&strsvr,(char**)cmd);

    for (int i=0;i<3;i++) {
        if (ConvEna[i]) strconvfree(strsvr.conv[i]);
    }
    if (TraceLevel>0) traceclose();
}
// load options -------------------------------------------------------------
void Strsvr::LoadOpt(void)
{
    int optdef[]={10000,10000,1000,32768,10,0};
/*初始化配置set,相当于配置文件的set一栏*/
    Input  = 1 ;
    Output1 = 3;
    Output2 = 0;
    Output3 = 0;
    TraceLevel  = 0 ;
    NmeaReq  = 0;
    FileSwapMargin = 30;
    StaId = 0;
    StaSel = 0;
    AntType = "";
    RcvType  = "";

    for (int i=0;i<6;i++) {
        SvrOpt[i]=optdef[i];
    }
    for (int i=0;i<3;i++) {
        AntPos[i]= 0 ;
        AntOff[i]= 0 ;
    }
/*初始化配置conv．相当于配置文件的conv一栏*/
    for (int i=0;i<3;i++) {
        ConvEna[i]= 0 ;
        ConvInp[i]= 0 ;
        ConvOut[i]= 0 ;
        ConvMsg[i]= "";
        ConvOpt[i]= "";
    }
    for (int i=0;i<2;i++) {
         /*初始化serial下的cmdena*/
        CmdEna   [i]= 1 ;
        /*初始化tcpip下的cmdena*/
        CmdEnaTcp[i]= 1 ;
    }
    /*初始化serial下的cmd*/
    for (int i=0;i<2;i++) {
        Cmds[i]= "";
       //Cmds[i]=Cmds[i].replace("@@","\n");
    }
    /*初始化tcpip下的cmd*/
    for (int i=0;i<2;i++) {
        CmdsTcp[i]="";
        //CmdsTcp[i]=CmdsTcp[i].replace("@@","\n");
    }
    /*    初始化Paths[][]*/
    Paths[0][0] = "";
    Paths[0][1] = ":@124.205.216.26:35946/::";
    Paths[0][2] = "";
    Paths[0][3] = "";

    Paths[1][0] = "";
    Paths[1][1] = ":@12020/:";
    Paths[1][2] = "";
    Paths[1][3] = "";

    Paths[2][0] = "";
    Paths[2][1] = ":@12020/:";
    Paths[2][2] = "";
    Paths[2][3] = "";

    Paths[3][0] = "";
    Paths[3][1] = "";
    Paths[3][2] = "";
    Paths[3][3] = "";
    /*初始化tcpopt*/
    for (int i=0;i<MAXHIST;i++) {
        TcpHistory[i]="";
    }
    for (int i=0;i<MAXHIST;i++) {
        TcpMntpHist[i]="";
    }
    /*初始化stapos*/
    StaPosFile    = "";
    /*初始化dirs*/
    ExeDirectory  ="";
    LocalDirectory="";
    ProxyAddress  ="";
}
//---------------------------------------------------------------------------
