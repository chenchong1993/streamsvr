/*------------------------------------------------------------------------------
* pntpos.c : standard positioning
*
*          Copyright (C) 2007-2018 by T.TAKASU, All rights reserved.
*
* version : $Revision:$ $Date:$
* history : 2010/07/28 1.0  moved from rtkcmn.c
*                           changed api:
*                               pntpos()
*                           deleted api:
*                               pntvel()
*           2011/01/12 1.1  add option to include unhealthy satellite
*                           reject duplicated observation data
*                           changed api: ionocorr()
*           2011/11/08 1.2  enable snr mask for single-mode (rtklib_2.4.1_p3)
*           2012/12/25 1.3  add variable snr mask
*           2014/05/26 1.4  support galileo and beidou
*           2015/03/19 1.5  fix bug on ionosphere correction for GLO and BDS
*           2018/10/10 1.6  support api change of satexclude()
*-----------------------------------------------------------------------------*/
#include "rtklib.h"

/* constants -----------------------------------------------------------------*/

#define SQR(x)      ((x)*(x))

#define NX          (4+3)       /* # of estimated parameters */

#define MAXITR      10          /* max number of iteration for point pos */
#define ERR_ION     5.0         /* ionospheric delay std (m) */
#define ERR_TROP    3.0         /* tropspheric delay std (m) */
#define ERR_SAAS    0.3         /* saastamoinen model error std (m) */
#define ERR_BRDCI   0.5         /* broadcast iono model error factor */
#define ERR_CBIAS   0.3         /* code bias error std (m) */
#define REL_HUMI    0.7         /* relative humidity for saastamoinen model */

/* pseudorange measurement error variance ------------------------------------*/
static double varerr(const prcopt_t *opt, double el, int sys)
{
    double fact,varr;
    fact=sys==SYS_GLO?EFACT_GLO:(sys==SYS_SBS?EFACT_SBS:EFACT_GPS);
    varr=SQR(opt->err[0])*(SQR(opt->err[1])+SQR(opt->err[2])/sin(el));
    if (opt->ionoopt==IONOOPT_IFLC) varr*=SQR(3.0); /* iono-free */
    return SQR(fact)*varr;
}
/* get tgd parameter (m) -----------------------------------------------------*/
static double gettgd(int sat, const nav_t *nav)
{
    int i;
    for (i=0;i<nav->n;i++) {
        if (nav->eph[i].sat!=sat) continue;
        return CLIGHT*nav->eph[i].tgd[0];
    }
    return 0.0;
}
/* psendorange with code bias correction -------------------------------------*/
static double prange(const obsd_t *obs, const nav_t *nav, const double *azel,
                     int iter, const prcopt_t *opt, double *var)
{
    const double *lam=nav->lam[obs->sat-1];
    double PC,P1,P2,P1_P2,P1_C1,P2_C2,gamma;
    int i=0,j=1,sys;

    *var=0.0;

    if (!(sys=satsys(obs->sat,NULL))) return 0.0;

    /* L1-L2 for GPS/GLO/QZS, L1-L5 for GAL/SBS */
    if (NFREQ>=3&&(sys&(SYS_GAL|SYS_SBS))) j=2;

    if (NFREQ<2||lam[i]==0.0||lam[j]==0.0) return 0.0;

    /* test snr mask */
    if (iter>0) {
        if (testsnr(0,i,azel[1],obs->SNR[i]*0.25,&opt->snrmask)) {
            trace(4,"snr mask: %s sat=%2d el=%.1f snr=%.1f\n",
                  time_str(obs->time,0),obs->sat,azel[1]*R2D,obs->SNR[i]*0.25);
            return 0.0;
        }
        if (opt->ionoopt==IONOOPT_IFLC) {
            if (testsnr(0,j,azel[1],obs->SNR[j]*0.25,&opt->snrmask)) return 0.0;
        }
    }
    gamma=SQR(lam[j])/SQR(lam[i]); /* f1^2/f2^2 */
    P1=obs->P[i];
    P2=obs->P[j];
    P1_P2=nav->cbias[obs->sat-1][0];
    P1_C1=nav->cbias[obs->sat-1][1];
    P2_C2=nav->cbias[obs->sat-1][2];

    /* if no P1-P2 DCB, use TGD instead */
    if (P1_P2==0.0&&(sys&(SYS_GPS|SYS_GAL|SYS_QZS))) {
        P1_P2=(1.0-gamma)*gettgd(obs->sat,nav);
    }
    if (opt->ionoopt==IONOOPT_IFLC) { /* dual-frequency */

        if (P1==0.0||P2==0.0) return 0.0;
        if (obs->code[i]==CODE_L1C) P1+=P1_C1; /* C1->P1 */
        if (obs->code[j]==CODE_L2C) P2+=P2_C2; /* C2->P2 */

        /* iono-free combination */
        PC=(gamma*P1-P2)/(gamma-1.0);
    }
    else { /* single-frequency */

        if (P1==0.0) return 0.0;
        if (obs->code[i]==CODE_L1C) P1+=P1_C1; /* C1->P1 */
        PC=P1-P1_P2/(1.0-gamma);
    }
    if (opt->sateph==EPHOPT_SBAS) PC-=P1_C1; /* sbas clock based C1 */

    *var=SQR(ERR_CBIAS);

    return PC;
}
/* ionospheric correction ------------------------------------------------------
* compute ionospheric correction
* args   : gtime_t time     I   time
*          nav_t  *nav      I   navigation data
*          int    sat       I   satellite number
*          double *pos      I   receiver position {lat,lon,h} (rad|m)
*          double *azel     I   azimuth/elevation angle {az,el} (rad)
*          int    ionoopt   I   ionospheric correction option (IONOOPT_???)
*          double *ion      O   ionospheric delay (L1) (m)
*          double *var      O   ionospheric delay (L1) variance (m^2)
* return : status(1:ok,0:error)
*-----------------------------------------------------------------------------*/
extern int ionocorr(gtime_t time, const nav_t *nav, int sat, const double *pos,
                    const double *azel, int ionoopt, double *ion, double *var)
{
    trace(4,"ionocorr: time=%s opt=%d sat=%2d pos=%.3f %.3f azel=%.3f %.3f\n",
          time_str(time,3),ionoopt,sat,pos[0]*R2D,pos[1]*R2D,azel[0]*R2D,
          azel[1]*R2D);

    /* broadcast model */
    if (ionoopt==IONOOPT_BRDC) {
        *ion=ionmodel(time,nav->ion_gps,pos,azel);
        *var=SQR(*ion*ERR_BRDCI);
        return 1;
    }
    /* sbas ionosphere model */
    if (ionoopt==IONOOPT_SBAS) {
        return sbsioncorr(time,nav,pos,azel,ion,var);
    }
    /* ionex tec model */
    if (ionoopt==IONOOPT_TEC) {
        return iontec(time,nav,pos,azel,1,ion,var);
    }
    /* qzss broadcast model */
    if (ionoopt==IONOOPT_QZS&&norm(nav->ion_qzs,8)>0.0) {
        *ion=ionmodel(time,nav->ion_qzs,pos,azel);
        *var=SQR(*ion*ERR_BRDCI);
        return 1;
    }
    /* lex ionosphere model */
    if (ionoopt==IONOOPT_LEX) {
        return lexioncorr(time,nav,pos,azel,ion,var);
    }
    *ion=0.0;
    *var=ionoopt==IONOOPT_OFF?SQR(ERR_ION):0.0;
    return 1;
}
/* tropospheric correction -----------------------------------------------------
* compute tropospheric correction
* args   : gtime_t time     I   time
*          nav_t  *nav      I   navigation data
*          double *pos      I   receiver position {lat,lon,h} (rad|m)
*          double *azel     I   azimuth/elevation angle {az,el} (rad)
*          int    tropopt   I   tropospheric correction option (TROPOPT_???)
*          double *trp      O   tropospheric delay (m)
*          double *var      O   tropospheric delay variance (m^2)
* return : status(1:ok,0:error)
*-----------------------------------------------------------------------------*/
extern int tropcorr(gtime_t time, const nav_t *nav, const double *pos,
                    const double *azel, int tropopt, double *trp, double *var)
{
    trace(4,"tropcorr: time=%s opt=%d pos=%.3f %.3f azel=%.3f %.3f\n",
          time_str(time,3),tropopt,pos[0]*R2D,pos[1]*R2D,azel[0]*R2D,
          azel[1]*R2D);

    /* saastamoinen model */
    if (tropopt==TROPOPT_SAAS||tropopt==TROPOPT_EST||tropopt==TROPOPT_ESTG) {
        *trp=tropmodel(time,pos,azel,REL_HUMI);
        *var=SQR(ERR_SAAS/(sin(azel[1])+0.1));
        return 1;
    }
    /* sbas troposphere model */
    if (tropopt==TROPOPT_SBAS) {
        *trp=sbstropcorr(time,pos,azel,var);
        return 1;
    }
    /* no correction */
    *trp=0.0;
    *var=tropopt==TROPOPT_OFF?SQR(ERR_TROP):0.0;
    return 1;
}
/* pseudorange residuals -----------------------------------------------------*/
static int rescode(int iter, const obsd_t *obs, int n, const double *rs,
                   const double *dts, const double *vare, const int *svh,
                   const nav_t *nav, const double *x, const prcopt_t *opt,
                   double *v, double *H, double *var, double *azel, int *vsat,
                   double *resp, int *ns)
{
    double r,dion,dtrp,vmeas,vion,vtrp,rr[3],pos[3],dtr,e[3],P,lam_L1;
    int i,j,nv=0,sys,mask[4]={0};

    trace(3,"resprng : n=%d\n",n);
    /*将之前得到的定位解信息赋值给 rr和dtr数组，以进行关于当前解的伪距残余的相关计算。*/
    for (i=0;i<3;i++) rr[i]=x[i];
    dtr=x[3];
    /*调用 ecef2pos函数，将上一步中得到的位置信息由 ECEF转化为大地坐标系。*/
    ecef2pos(rr,pos);

    for (i=*ns=0;i<n&&i<MAXOBS;i++) {
        /*将 vsat、azel和 resp数组置 0，因为在前后两次定位结果中，每颗卫星的上述信息都会发生变化。*/
        vsat[i]=0; azel[i*2]=azel[1+i*2]=resp[i]=0.0;
        /*调用 satsys函数，验证卫星编号是否合理及其所属的导航系统。*/
        if (!(sys=satsys(obs[i].sat,NULL))) continue;

        /* reject duplicated observation data
        检测当前观测卫星是否和下一个相邻数据重复。是，则 i++后继续下一次循环；否，则进入下一步。 */
        if (i<n-1&&i<MAXOBS-1&&obs[i].sat==obs[i+1].sat) {
            trace(2,"duplicated observation data %s sat=%2d\n",
                  time_str(obs[i].time,3),obs[i].sat);
            i++;
            continue;
        }
        /* geometric distance/azimuth/elevation angle
        调用 geodist函数，计算卫星和当前接收机位置之间的几何距离和
           receiver-to-satellite方向的单位向量。然后检验几何距离是否 >0。
        调用 satazel函数，计算在接收机位置处的站心坐标系中卫星的方位角和仰角，检验仰角是否 $\geq$截断值。*/
        if ((r=geodist(rs+i*6,rr,e))<=0.0||
            satazel(pos,e,azel+i*2)<opt->elmin) continue;

        /* psudorange with code bias correction
        调用 prange函数，得到伪距值。*/
        if ((P=prange(obs+i,nav,azel+i*2,iter,opt,&vmeas))==0.0) continue;

        /* excluded satellite?
        可以在处理选项中事先指定只选用哪些导航系统或卫星来进行定位，这是通过调用 satexclude函数完成的。*/
        if (satexclude(obs[i].sat,vare[i],svh[i],opt)) continue;

        /* ionospheric corrections
        调用 ionocorr函数，计算电离层延时(m)。*/
        if (!ionocorr(obs[i].time,nav,obs[i].sat,pos,azel+i*2,
                      iter>0?opt->ionoopt:IONOOPT_BRDC,&dion,&vion)) continue;

        /* GPS-L1 -> L1/B1
        上一步中所得的电离层延时是建立在 L1信号上的，当使用其它频率信号时，
        依据所用信号频组中第一个频率的波长与 L1波长的关系，对上一步得到的电离层延时进行修正。 */
        if ((lam_L1=nav->lam[obs[i].sat-1][0])>0.0) {
            dion*=SQR(lam_L1/lam_carr[0]);
        }
        /* tropospheric corrections
        调用 tropcorr函数，计算对流层延时(m)。*/
        if (!tropcorr(obs[i].time,nav,pos,azel+i*2,
                      iter>0?opt->tropopt:TROPOPT_SAAS,&dtrp,&vtrp)) {
            continue;
        }
        /* pseudorange residual
        计算出此时的伪距残余。*/
        v[nv]=P-(r+dtr-CLIGHT*dts[i*2]+dion+dtrp);

        /* design matrix
        组装几何矩阵，前 3行为 6中计算得到的视线单位向量的反向，第 4行为 1，其它行为 0。*/
        for (j=0;j<NX;j++) H[j+nv*NX]=j<3?-e[j]:(j==3?1.0:0.0);

        /* time system and receiver bias offset correction
        将参与定位的卫星的定位有效性标志设为 1，给当前卫星的伪距残余赋值，参与定位的卫星个数 ns加 1.*/
        if      (sys==SYS_GLO) {v[nv]-=x[4]; H[4+nv*NX]=1.0; mask[1]=1;}
        else if (sys==SYS_GAL) {v[nv]-=x[5]; H[5+nv*NX]=1.0; mask[2]=1;}
        else if (sys==SYS_CMP) {v[nv]-=x[6]; H[6+nv*NX]=1.0; mask[3]=1;}
        else mask[0]=1;

        vsat[i]=1; resp[i]=v[nv]; (*ns)++;

        /* error variance
        调用 varerr函数，计算此时的导航系统误差（可能会包括 IFLC选项时的电离层延时），然后累加计算用户测距误差(URE)。*/
        var[nv++]=varerr(opt,azel[1+i*2],sys)+vare[i]+vmeas+vion+vtrp;

        trace(4,"sat=%2d azel=%5.1f %4.1f res=%7.3f sig=%5.3f\n",obs[i].sat,
              azel[i*2]*R2D,azel[1+i*2]*R2D,resp[i],sqrt(var[nv-1]));
    }
    /* constraint to avoid rank-deficient
    为了防止亏秩，人为的添加了几组观测方程。*/
    for (i=0;i<4;i++) {
        if (mask[i]) continue;
        v[nv]=0.0;
        for (j=0;j<NX;j++) H[j+nv*NX]=j==i+3?1.0:0.0;
        var[nv++]=0.01;
    }
    return nv;
}
/* validate solution ---------------------------------------------------------*/
static int valsol(const double *azel, const int *vsat, int n,
                  const prcopt_t *opt, const double *v, int nv, int nx,
                  char *msg)
{
    double azels[MAXOBS*2],dop[4],vv;
    int i,ns;

    trace(3,"valsol  : n=%d nv=%d\n",n,nv);

    /* chi-square validation of residuals
    先计算定位后伪距残余平方加权和 vv。*/
    vv=dot(v,v,nv);
    /*检查是否满足 $vv>\chi^2(nv-nx-1)$。是，则说明此时的定位解误差过大，返回 0；否则，转到下一步。*/
    if (nv>nx&&vv>chisqr[nv-nx-1]) {
        sprintf(msg,"chi-square error nv=%d vv=%.1f cs=%.1f",nv,vv,chisqr[nv-nx-1]);
        return 0;
    }
    /* large gdop check
    复制 azel，这里只复制那些对于定位结果有贡献的卫星的 zael值，并且统计实现定位时所用卫星的数目。*/
    for (i=ns=0;i<n;i++) {
        if (!vsat[i]) continue;
        azels[  ns*2]=azel[  i*2];
        azels[1+ns*2]=azel[1+i*2];
        ns++;
    }
    /*调用 dops函数，计算各种精度因子(DOP)，检验是否有 0<GDOP<max。否，则说明该定位解的精度不符合要求，返回 0；是，则返回 1。*/
    dops(ns,azels,opt->elmin,dop);
    if (dop[0]<=0.0||dop[0]>opt->maxgdop) {
        sprintf(msg,"gdop error nv=%d gdop=%.1f",nv,dop[0]);
        return 0;
    }
    return 1;
}
/* estimate receiver position ------------------------------------------------*/
static int estpos(const obsd_t *obs, int n, const double *rs, const double *dts,
                  const double *vare, const int *svh, const nav_t *nav,
                  const prcopt_t *opt, sol_t *sol, double *azel, int *vsat,
                  double *resp, char *msg)
{
    double x[NX]={0},dx[NX],Q[NX*NX],*v,*H,*var,sig;
    int i,j,k,info,stat,nv,ns;

    trace(3,"estpos  : n=%d\n",n);

    v=mat(n+4,1); H=mat(NX,n+4); var=mat(n+4,1);
    /*1. 将 sol->rr的前 3项赋值给 x数组。*/
    for (i=0;i<3;i++) x[i]=sol->rr[i];

    for (i=0;i<MAXITR;i++) {

        /* 2. pseudorange residuals
        开始迭代定位计算，首先调用 rescode函数，计算在当前接收机位置和钟差值的情况下，
        定位方程的右端部分 v(nv\*1)、几何矩阵 H(NX*nv)、此时所得的伪距残余的方差 var、
        所有观测卫星的 azel{方位角、仰角}、定位时有效性 vsat、定位后伪距残差 resp、
        参与定位的卫星个数 ns和方程个数 nv。*/
        nv=rescode(i,obs,n,rs,dts,vare,svh,nav,x,opt,v,H,var,azel,vsat,resp,
                   &ns);
        /*3. 确定方程组中方程的个数要大于未知数的个数。*/
        if (nv<NX) {
            sprintf(msg,"lack of valid sats ns=%d",nv);
            break;
        }
        /* 4. weight by variance
        以伪距残差的倒数作为权重，对 H和 v分别左乘权重对角阵，得到加权之后的 H和 v。*/
        for (j=0;j<nv;j++) {
            sig=sqrt(var[j]);
            v[j]/=sig;
            for (k=0;k<NX;k++) H[k+j*NX]/=sig;
        }
        /* 5. least square estimation 最小二乘
        调用 lsq函数，得到当前 x的改正数和定位误差协方差矩阵中的权系数阵。*/
        if ((info=lsq(H,v,NX,nv,dx,Q))) {
            sprintf(msg,"lsq error info=%d",info);
            break;
        }
        /*6. 将 5中求得的 dx加入到当前 x值中，得到更新之后的 x值。*/
        for (j=0;j<NX;j++) x[j]+=dx[j];
/*        7. 如果两次定位结果之差小于阈值(目前是1e-4)，则将 6中得到的 x值作为最终的定位结果，
        对 sol的相应参数赋值，之后再调用 valsol函数确认当前解是否符合要求
        （伪距残余小于某个 $\chi^2$值和 GDOP小于某个门限值）。否则，进行下一次循环。*/
        if (norm(dx,NX)<1E-4) {
            sol->type=0;
            sol->time=timeadd(obs[0].time,-x[3]/CLIGHT);
            sol->dtr[0]=x[3]/CLIGHT; /* receiver clock bias (s) */
            sol->dtr[1]=x[4]/CLIGHT; /* glo-gps time offset (s) */
            sol->dtr[2]=x[5]/CLIGHT; /* gal-gps time offset (s) */
            sol->dtr[3]=x[6]/CLIGHT; /* bds-gps time offset (s) */
            for (j=0;j<6;j++) sol->rr[j]=j<3?x[j]:0.0;
            for (j=0;j<3;j++) sol->qr[j]=(float)Q[j+j*NX];
            sol->qr[3]=(float)Q[1];    /* cov xy */
            sol->qr[4]=(float)Q[2+NX]; /* cov yz */
            sol->qr[5]=(float)Q[2];    /* cov zx */
            sol->ns=(unsigned char)ns;
            sol->age=sol->ratio=0.0;

            /* validate solution
            确认当前解是否符合要求*/
            if ((stat=valsol(azel,vsat,n,opt,v,nv,NX,msg))) {
                sol->stat=opt->sateph==EPHOPT_SBAS?SOLQ_SBAS:SOLQ_SINGLE;
            }
            free(v); free(H); free(var);

            return stat;
        }
    }
    /*如果超过了规定的循环次数，则输出发散信息后，返回 0*/
    if (i>=MAXITR) sprintf(msg,"iteration divergent i=%d",i);

    free(v); free(H); free(var);

    return 0;
}
/* raim fde (failure detection and exclution) -------------------------------*/
static int raim_fde(const obsd_t *obs, int n, const double *rs,
                    const double *dts, const double *vare, const int *svh,
                    const nav_t *nav, const prcopt_t *opt, sol_t *sol,
                    double *azel, int *vsat, double *resp, char *msg)
{
    obsd_t *obs_e;
    sol_t sol_e={{0}};
    char tstr[32],name[16],msg_e[128];
    double *rs_e,*dts_e,*vare_e,*azel_e,*resp_e,rms_e,rms=100.0;
    int i,j,k,nvsat,stat=0,*svh_e,*vsat_e,sat=0;

    trace(3,"raim_fde: %s n=%2d\n",time_str(obs[0].time,0),n);

    if (!(obs_e=(obsd_t *)malloc(sizeof(obsd_t)*n))) return 0;
    rs_e = mat(6,n); dts_e = mat(2,n); vare_e=mat(1,n); azel_e=zeros(2,n);
    svh_e=imat(1,n); vsat_e=imat(1,n); resp_e=mat(1,n);

    for (i=0;i<n;i++) {

        /* satellite exclution
        关于观测卫星数目的循环，每次舍弃一颗卫星，计算使用余下卫星进行定位的定位值。*/
        for (j=k=0;j<n;j++) {
            if (j==i) continue;
            obs_e[k]=obs[j];
            matcpy(rs_e +6*k,rs +6*j,6,1);
            matcpy(dts_e+2*k,dts+2*j,2,1);
            vare_e[k]=vare[j];
            svh_e[k++]=svh[j];
        }
        /* estimate receiver position without a satellite
        舍弃一颗卫星后，将剩下卫星的数据复制到一起，调用 estpos函数，计算使用余下卫星进行定位的定位值。*/
        if (!estpos(obs_e,n-1,rs_e,dts_e,vare_e,svh_e,nav,opt,&sol_e,azel_e,
                    vsat_e,resp_e,msg_e)) {
            trace(3,"raim_fde: exsat=%2d (%s)\n",obs[i].sat,msg);
            continue;
        }
        /*累加使用当前卫星实现定位后的伪距残差平方和与可用卫星数目。*/
        for (j=nvsat=0,rms_e=0.0;j<n-1;j++) {
            if (!vsat_e[j]) continue;
            rms_e+=SQR(resp_e[j]);
            nvsat++;
        }
        /*如果 nvsat<5，则说明当前卫星数目过少，无法进行 RAIM_FDE操作*/
        if (nvsat<5) {
            trace(3,"raim_fde: exsat=%2d lack of satellites nvsat=%2d\n",
                  obs[i].sat,nvsat);
            continue;
        }
        /*计算伪距残差平方和的标准平均值*/
        rms_e=sqrt(rms_e/nvsat);

        trace(3,"raim_fde: exsat=%2d rms=%8.3f\n",obs[i].sat,rms_e);
       /* 如果大于 rms，则说明当前定位结果更合理，将 stat置为 1，*/
        if (rms_e>rms) continue;

        /* save result
        重新更新 sol、azel、vsat(当前被舍弃的卫星，此值置为0)、resp等值，并将当前的 rms_e更新到 `rms'中。*/
        for (j=k=0;j<n;j++) {
            if (j==i) continue;
            matcpy(azel+2*j,azel_e+2*k,2,1);
            vsat[j]=vsat_e[k];
            resp[j]=resp_e[k++];
        }
        stat=1;
        *sol=sol_e;
        sat=obs[i].sat;
        rms=rms_e;
        vsat[i]=0;
        strcpy(msg,msg_e);
       /* 继续弃用下一颗卫星，重复操作。
        总而言之，将同样是弃用一颗卫星条件下，伪距残差标准平均值最小的组合所得的结果作为最终的结果输出。*/
    }
    /*如果 stat不为 0，则说明在弃用卫星的前提下有更好的解出现，输出信息，指出弃用了哪颗卫星。*/
    if (stat) {
        time2str(obs[0].time,tstr,2); satno2id(sat,name);
        trace(2,"%s: %s excluded by raim\n",tstr+11,name);
    }
    free(obs_e);
    free(rs_e ); free(dts_e ); free(vare_e); free(azel_e);
    free(svh_e); free(vsat_e); free(resp_e);
    return stat;
}
/* doppler residuals ---------------------------------------------------------*/
static int resdop(const obsd_t *obs, int n, const double *rs, const double *dts,
                  const nav_t *nav, const double *rr, const double *x,
                  const double *azel, const int *vsat, double *v, double *H)
{
    double lam,rate,pos[3],E[9],a[3],e[3],vs[3],cosel;
    int i,j,nv=0;

    trace(3,"resdop  : n=%d\n",n);

    ecef2pos(rr,pos); xyz2enu(pos,E);

    for (i=0;i<n&&i<MAXOBS;i++) {

        lam=nav->lam[obs[i].sat-1][0];

        if (obs[i].D[0]==0.0||lam==0.0||!vsat[i]||norm(rs+3+i*6,3)<=0.0) {
            continue;
        }
        /* line-of-sight vector in ecef */
        cosel=cos(azel[1+i*2]);
        a[0]=sin(azel[i*2])*cosel;
        a[1]=cos(azel[i*2])*cosel;
        a[2]=sin(azel[1+i*2]);
        matmul("TN",3,1,3,1.0,E,a,0.0,e);

        /* satellite velocity relative to receiver in ecef */
        for (j=0;j<3;j++) vs[j]=rs[j+3+i*6]-x[j];

        /* range rate with earth rotation correction */
        rate=dot(vs,e,3)+OMGE/CLIGHT*(rs[4+i*6]*rr[0]+rs[1+i*6]*x[0]-
                                      rs[3+i*6]*rr[1]-rs[  i*6]*x[1]);

        /* doppler residual */
        v[nv]=-lam*obs[i].D[0]-(rate+x[3]-CLIGHT*dts[1+i*2]);

        /* design matrix */
        for (j=0;j<4;j++) H[j+nv*4]=j<3?-e[j]:1.0;

        nv++;
    }
    return nv;
}
/* estimate receiver velocity ------------------------------------------------*/
static void estvel(const obsd_t *obs, int n, const double *rs, const double *dts,
                   const nav_t *nav, const prcopt_t *opt, sol_t *sol,
                   const double *azel, const int *vsat)
{
    double x[4]={0},dx[4],Q[16],*v,*H;
    int i,j,nv;

    trace(3,"estvel  : n=%d\n",n);

    v=mat(n,1); H=mat(4,n);

    for (i=0;i<MAXITR;i++) {

        /* doppler residuals
        在最大迭代次数限制内，调用 resdop，计算定速方程组左边的几何矩阵和右端的速度残余，返回定速时所使用的卫星数目。 */
        if ((nv=resdop(obs,n,rs,dts,nav,sol->rr,x,azel,vsat,v,H))<4) {
            break;
        }
        /* least square estimation
        调用 lsq函数，解出 {速度、频漂}的步长，累加到 x中。 */
        if (lsq(H,v,4,nv,dx,Q)) break;

        for (j=0;j<4;j++) x[j]+=dx[j];
        /*检查当前计算出的步长的绝对值是否小于 1E-6。
        是，则说明当前解已经很接近真实值了，将接收机三个方向上的速度存入到 sol_t.rr中；
        否，则进行下一次循环*/
        if (norm(dx,4)<1E-6) {
            for (i=0;i<3;i++) sol->rr[i+3]=x[i];
            break;
        }
    }
   /* 执行完所有迭代次数，使用 free回收内存。*/
    free(v); free(H);
}
/* single-point positioning ----------------------------------------------------
* compute receiver position, velocity, clock bias by single-point positioning
* with pseudorange and doppler observables
* args   : obsd_t *obs      I   observation data
*          int    n         I   number of observation data
*          nav_t  *nav      I   navigation data
*          prcopt_t *opt    I   processing options
*          sol_t  *sol      IO  solution
*          double *azel     IO  azimuth/elevation angle (rad) (NULL: no output)
*          ssat_t *ssat     IO  satellite status              (NULL: no output)
*          char   *msg      O   error message for error exit
* return : status(1:ok,0:error)
* notes  : assuming sbas-gps, galileo-gps, qzss-gps, compass-gps time offset and
*          receiver bias are negligible (only involving glonass-gps time offset
*          and receiver bias)
*-----------------------------------------------------------------------------*/
extern int pntpos(const obsd_t *obs, int n, const nav_t *nav,
                  const prcopt_t *opt, sol_t *sol, double *azel, ssat_t *ssat,
                  char *msg)
{
    prcopt_t opt_=*opt;
    double *rs,*dts,*var,*azel_,*resp;
    int i,stat,vsat[MAXOBS]={0},svh[MAXOBS];

    sol->stat=SOLQ_NONE;

    if (n<=0) {strcpy(msg,"no observation data"); return 0;}

    trace(3,"pntpos  : tobs=%s n=%d\n",time_str(obs[0].time,3),n);

    sol->time=obs[0].time; msg[0]='\0';

    rs=mat(6,n); dts=mat(2,n); var=mat(1,n); azel_=zeros(2,n); resp=mat(1,n);

    if (opt_.mode!=PMODE_SINGLE) { /* for precise positioning */
#if 0
        opt_.sateph =EPHOPT_BRDC;
#endif
        opt_.ionoopt=IONOOPT_BRDC;
        opt_.tropopt=TROPOPT_SAAS;
    }
    /* satellite positons, velocities and clocks
    调用 satposs函数，按照所观测到的卫星顺序计算出每颗卫星的位置、速度、{钟差、频漂}。*/
    satposs(sol->time,obs,n,nav,opt_.sateph,rs,dts,var,svh);

    /* estimate receiver position with pseudorange
    调用 estpos函数，通过伪距实现绝对定位，计算出接收机的位置和钟差，
    顺带返回实现定位后每颗卫星的{方位角、仰角}、定位时有效性、定位后伪距残差*/
    stat=estpos(obs,n,rs,dts,var,svh,nav,&opt_,sol,azel_,vsat,resp,msg);

    /* raim fde
    调用 raim_fde函数，对上一步得到的定位结果进行接收机自主完备性检测（RAIM）。
    通过再次使用 vsat数组，这里只会在对定位结果有贡献的卫星数据进行检测。 */
    if (!stat&&n>=6&&opt->posopt[4]) {
        stat=raim_fde(obs,n,rs,dts,var,svh,nav,&opt_,sol,azel_,vsat,resp,msg);
    }
    /* estimate receiver velocity with doppler
    调用 estvel函数，依靠多普勒频移测量值计算接收机的速度。
    这里只使用通过了上一步 RAIM_FDE操作的卫星数据，所以对于计算出的速度就没有再次进行 RAIM了。 */
    if (stat) estvel(obs,n,rs,dts,nav,&opt_,sol,azel_,vsat);
/*    首先将 ssat_t结构体数组的 vs(定位时有效性)、azel（方位角、仰角）、resp(伪距残余)、
    resc(载波相位残余)和 snr(信号强度)都置为 0，然后再将实现定位后的 azel、snr赋予 ssat_t结构体数组，
    而 vs、resp则只赋值给那些对定位有贡献的卫星，没有参与定位的卫星，这两个属性值为 0。*/
    if (azel) {
        for (i=0;i<n*2;i++) azel[i]=azel_[i];
    }
    if (ssat) {
        for (i=0;i<MAXSAT;i++) {
            ssat[i].vs=0;
            ssat[i].azel[0]=ssat[i].azel[1]=0.0;
            ssat[i].resp[0]=ssat[i].resc[0]=0.0;
            ssat[i].snr[0]=0;
        }
        for (i=0;i<n;i++) {
            ssat[obs[i].sat-1].azel[0]=azel_[  i*2];
            ssat[obs[i].sat-1].azel[1]=azel_[1+i*2];
            ssat[obs[i].sat-1].snr[0]=obs[i].SNR[0];
            if (!vsat[i]) continue;
            ssat[obs[i].sat-1].vs=1;
            ssat[obs[i].sat-1].resp[0]=resp[i];
        }
    }
    free(rs); free(dts); free(var); free(azel_); free(resp);
    return stat;
}
