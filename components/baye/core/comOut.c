/***********************************************************************
 *Copyright (c)2005 , 东莞步步高教育电子分公司
 *All rights reserved.
 **
 文件名称：	comOut.c
 *文件标识：	步步高电子词典的游戏引擎模块
 *摘要：		封装引擎对系统显示程序调用的入口
 **
 *移植性声明:
 *	1、符合标准：《游戏设计标准V1.0》
 *	2、兼容模式：本程序和界面无关，无兼容模式。
 *	3、使用本封装程序的引擎要移植到其他环境中，系统的显示部分只需在此修改
 *修改历史：
 *	版本    日期     作者     改动内容和原因
 *   ----------------------------------------------------
 *	1.0    2005.5.16  高国军     基本的功能完成
 ***********************************************************************/
/* 声明本文件可以直接调用系统底层函数宏 */
#define		_BE_CHANGED_
#include "baye/stdsys.h"
#include "baye/comm.h"
#include "baye/enghead.h"
#include "touch.h"

/* 当前所在文件 */
#define		IN_FILE		21

/*本体函数声明*/
/*------------------------------------------*/
U32	CountHZMAddrOff(U16 Hz);
void	GamResumeSet(U16 bakBnk);
void	GamAscii(U8 x,U8 y,U8 asc);
void	GamChinese(U8 x,U8 y,U16 Hz);
U32	GamStrShow(U8 x,U8 y,const U8 *buf);
void	GetExcHZMCode(U16 Hz,U8 *hzmCode);

/***********************************************************************
 * 说明:     游戏系统信息框(不保存背景)
 * 输入参数: buf-信息内容	delay-延时(秒)
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamMsgBox(const U8 *buf,U8 delay)
{
    U8	h,s;
    U8	slen;

    c_Sx = WK_SX + 5;
    c_Ex = WK_EX - 4;
    slen = gam_strlen(buf);
    h = (WK_EX - WK_SX) / ASC_WID - 2;
    s = slen / h + 1;
    if(s > (WK_EY - WK_SY) / ASC_HGT - 1)
    {
        c_Sy = WK_SY+5;
        c_Ey = WK_EY-5;
    }
    else
    {
        s *= ASC_HGT;
        c_Sy = (WK_EY - WK_SY - s) >> 1;
        c_Sy += WK_SY;
        c_Ey = c_Sy + s;
    }
    gam_clrlcd(c_Sx-3,c_Sy-3,c_Ex+3,c_Ey+2);
    gam_rect(c_Sx-3,c_Sy-3,c_Ex+3,c_Ey+2);
    GamStrShowS(c_Sx,c_Sy,buf);
    if(delay == 0)
        return;
    GamDelay(delay*100, 2);
}
/***********************************************************************
 * 说明:     显示虚拟屏幕到屏幕
 * 输入参数: vscr-虚拟屏幕指针
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamShowFrame(U8 *vscr)
{
    gam_Picture(0,0,SCR_WID-1,SCR_HGT-1,vscr);
}
/***********************************************************************
 * 说明:     显示图片到屏幕
 * 输入参数: x,y-显示坐标	wid,hgt-图片尺寸	pic-图片数据
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamPicShowS(U8 x,U8 y,U8 wid,U8 hgt,U8 *pic)
{
    wid-=1;
    hgt-=1;
    gam_Picture(x,y,x+wid,y+hgt,pic);
}
/***********************************************************************
 * 说明:     显示图片到虚拟屏幕
 * 输入参数: x,y-显示坐标	wid,hgt-图片尺寸	pic-图片数据
 *	   : vscr-虚拟屏幕指针
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamPicShowV(U8 x,U8 y,U8 wid,U8 hgt,U8 *pic,U8 *vscr)
{
    wid-=1;
    hgt-=1;
    GamePictureDummy(x,y,x+wid,y+hgt,pic,vscr,0);			/* 正常显示模式 */
}
/***********************************************************************
 * 说明:     显示mask图片到屏幕
 * 输入参数: x,y-显示坐标	wid,hgt-图片尺寸	pic-图片数据
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamMPicShowS(U8 x,U8 y,U8 wid,U8 hgt,U8 *pic)
{
    gam_Picture(x,y,x+wid,y+hgt,pic);
}
/***********************************************************************
 * 说明:     显示mask图片到虚拟屏幕
 * 输入参数: x,y-显示坐标	wid,hgt-图片尺寸	pic-图片数据
 *	   : vscr-虚拟屏幕指针
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamMPicShowV(U8 x,U8 y,U8 wid,U8 hgt,U8 *pic,U8 *vscr)
{
    U16 pLen;

    GamePictureDummy(x,y,x+wid-1,y+hgt-1,pic,vscr,1);		/* &显示模式 */
    pLen=wid>>3;
    if(wid&0x07) pLen+=1;
    pLen*=hgt;
    GamePictureDummy(x,y,x+wid-1,y+hgt-1,(U8 *)(pic+pLen),vscr,2);	/* |显示模式 */
}
/***********************************************************************
 * 说明:     显示图片到屏幕(功能扩展——可显示图片上面的部分)
 * 输入参数: x,y-显示坐标	wid,hgt-图片尺寸	idx-图片序号(从0开始)
 *	  : pic-图片数据(包括图片头)
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamPicShowExS(U8 x,U8 y,U8 wid,U8 hgt,U8 idx,U8 *pic)
{
    U8	mask;
    U8	pwid,phgt;
    U16	pLen;

    pwid = ((PictureHeadType *)pic)->wid;
    phgt = ((PictureHeadType *)pic)->hig;
    mask = ((PictureHeadType *)pic)->mask;
    pLen = pwid >> 3;
    if(pwid&0x07) pLen += 1;
    pLen *= phgt;
    pic += pLen * idx + PICHEAD_LEN;
    if(HV_MASK == mask)
        return;
    else
        GamPicShowS(x,y,wid,hgt,pic);
}
/***********************************************************************
 * 说明:     显示图片到虚拟屏幕(功能扩展——可显示图片上面的部分)
 * 输入参数: x,y-显示坐标	wid,hgt-图片尺寸	idx-图片序号(从0开始)
 *	  : pic-图片数据(包括图片头)	vscr-虚拟屏幕指针
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR void GamPicShowExV(U8 x,U8 y,U8 wid,U8 hgt,U8 idx,U8 *pic,U8 *vscr)
{
    U8	mask;
    U8	pwid,phgt;
    U16	pLen;

    pwid = ((PictureHeadType *)pic)->wid;
    phgt = ((PictureHeadType *)pic)->hig;
    mask = ((PictureHeadType *)pic)->mask & 0x01;
    pLen = pwid >> 3;
    if(pwid&0x07) pLen += 1;
    pLen *= phgt;
    pic += (pLen << mask) * idx + PICHEAD_LEN;
    if(HV_MASK == mask)
    {
        GamePictureDummy(x,y,x+wid-1,y+hgt-1,pic,vscr,1);	/* &显示模式 */
        pic += pLen;
        GamePictureDummy(x,y,x+wid-1,y+hgt-1,pic,vscr,2);	/* |显示模式 */
    }
    else
        GamPicShowV(x,y,wid,hgt,pic,vscr);
}
/***********************************************************************
 * 说明:     显示12字符串到屏幕
 * 输入参数: x,y-显示坐标	str-数据缓冲
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR U32 GamStrShowS(U8 x,U8 y,const U8 *str)
{
    U16 bakBnk;
    U32 rv;

    GetDataBankNumber(9,&bakBnk);
    c_VisScr=(U8 *)NULL;
    rv = GamStrShow(x,y,str);
    GamResumeSet(bakBnk);
    return rv;
}
/***********************************************************************
 * 说明:     显示12字符串到虚拟屏幕
 * 输入参数: x,y-显示坐标	str-数据缓冲	vscr-虚拟屏幕
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
FAR U32 GamStrShowV(U8 x,U8 y,U8 *str,U8 *vscr)
{
    U16 bakBnk;
    U32 rv;

    GetDataBankNumber(9,&bakBnk);
    c_VisScr=vscr;
    rv = GamStrShow(x,y,str);
    GamResumeSet(bakBnk);
    return rv;
}
/***********************************************************************
 * 说明:     初始化游戏引擎所在的机型环境
 * 输入参数: 无
 * 返回值  : 0-操作成功		!0-错误代码
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
void GamResumeSet(U16 bakBnk)
{
    DataBankSwitch(9,4,bakBnk);
    /*恢复字符串显示区域*/
    if(c_ReFlag)
    {
        c_Sx = WK_SX;
        c_Sy = WK_SY;
        c_Ex = WK_EX;
        c_Ey = WK_EY;
    }
}
/***********************************************************************
 * 说明:     显示12字符串
 * 输入参数: 无
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2005.5.16       完成基本功能
 ***********************************************************************/
U32 GamStrShow(U8 x,U8 y,const U8 *buf)
{
    U16	i,wid;
    U16	hzCode;

    if (y+HZ_HGT > c_Ey) return 0;

    for(i=0; buf[i] != '\0'; i++)
    {
        hzCode=buf[i];

        if(hzCode=='\n')
        {
            x=c_Sx;
            y+=HZ_HGT;
            if(y>c_Ey) break;
            continue;
        }
        if(hzCode<0x80)
            wid=ASC_WID-1;
        else
            wid=HZ_WID-1;
        if(x+wid>c_Ex)
            x+=wid;
        if(x>c_Ex)
        {
            x=c_Sx;
            if(x + wid > c_Ex) break;
            y+=HZ_HGT;
            if(y + HZ_HGT>c_Ey) break;
        }
        if(hzCode<0x80)
        {
            GamAscii(x,y,hzCode);
            x+=6;
        }
        else
        {
            i+=1;
            hzCode<<=8;
            hzCode+=buf[i];
            GamChinese(x,y,hzCode);
            x+=12;
        }
    }
    return i;
}
/***********************************************************************
 * 说明:     显示12*12点阵GB2312汉字
 * 输入参数: x,y	->显示坐标	Hz	->要显示的汉字内码
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2004.6.2        基本完成
 ***********************************************************************/
void GamChinese(U8 x,U8 y,U16 Hz)
{
    U8 zmCode[24];

    GetExcHZMCode(Hz,zmCode);
    if(c_VisScr==NULL)
        gam_Picture(x,y,x+HZ_WID-1,y+HZ_HGT-1,zmCode);
    else
        GamePictureDummy(x,y,x+HZ_WID-1,y+HZ_HGT-1,zmCode,c_VisScr,0);
}
/***********************************************************************
 * 说明:     显示12*12点阵GB2312AscII
 * 输入参数: x,y	->显示坐标	asc	->要显示的AscII
 * 返回值  :
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2004.6.2        基本完成
 ***********************************************************************/
void GamAscii(U8 x,U8 y,U8 asc)
{
    U16 ascCode;
    U8  i,zmCode[24];

    if(asc <= ' ')
        gam_memset(zmCode,0,24);
    else
    {
        ascCode=GAM_FONT_ASC_QU;
        ascCode<<=8;
        ascCode+=asc-0x21+0xA1;		/* 汉字字模从'!'开始*/

        GetExcHZMCode(ascCode,zmCode);
        for(i=0;i<12;i++)
            zmCode[i]=zmCode[i<<1];
    }

    if(c_VisScr==NULL)
        gam_Picture(x,y,x+ASC_WID-1,y+ASC_HGT-1,zmCode);
    else
        GamePictureDummy(x,y,x+ASC_WID-1,y+ASC_HGT-1,zmCode,c_VisScr,0);
}
/***********************************************************************
 * 说明:     获取扩充后的汉字字模数据(18->24)
 * 输入参数: Hz	->汉字内码	hzmCode	->扩充后的数据buf
 * 返回值  : 无
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2004.6.2        基本完成
 ***********************************************************************/
void GetExcHZMCode(U16 Hz,U8 *hzmCode)
{
    /*字模页号偏移 是否跨bank */
    U8  buf[18];
    U8  i,j,k;
    U32 hzmAddr;

    /* 当前要显示的汉字不是2312GB中的汉字，调试模式下显示黑块，释放模式下显示白块 */
    if((U8)(Hz>>8) < 0xA1)
    {
#if	GAM_VER==GAM_DEBUG_MODE
        gam_memset(hzmCode,0xFF,24);
        return;
#else
        gam_memset(hzmCode,0,24);
        return;
#endif
    }
    else
    {
        hzmAddr=CountHZMAddrOff(Hz);
        memset(buf, 18, 2);
        gam_fseek(g_FontFp,hzmAddr,SEEK_SET);
        gam_fread(buf,1,18,g_FontFp);
    }

    /*转换数据*/
    for(i=0;i<6;i++)
    {
        j=i<<2;
        k=(i<<1)+i;
        hzmCode[j]=buf[k];
        hzmCode[j+1]=buf[k+1]&0xf0;
        hzmCode[j+2]=(buf[k+1]&0x0f)<<4;
        hzmCode[j+2]+=(buf[k+2]&0xf0)>>4;
        hzmCode[j+3]=(buf[k+2]&0x0f)<<4;
    }
}
/***********************************************************************
 * 说明:     计算要显示的汉字字模地址偏移
 * 输入参数: Hz	->汉字内码
 * 返回值  : 该汉字字模在字库中的地址偏移
 * 修改历史:
 *               姓名            日期             说明
 *             ------          ----------      -------------
 *             高国军          2004.6.2        基本完成
 ***********************************************************************/
U32 CountHZMAddrOff(U16 Hz)
{
    /*计算公式:(94*(HCode-0xA1)+(LCode-0xA1))*18 */
    U32 offset,pCode;

    pCode=(U8)(Hz>>8)-0xA1;
    offset=pCode<<6;			/* offset=pCode*64 */
    offset+=pCode<<5;			/* offset=pCode*64+pCode*32 */
    offset=offset-(pCode<<1);		/* offset=pCode*94 */
    offset+=(U8)(Hz)-0xA1;			/* offset=94*(HCode-0xA1)+(LCode-0xA1)*/
    pCode=offset;
    offset<<=4;
    offset+=pCode<<1;
    return offset;
}

/******************************************************************************
 * 函数名:GamAsciiS
 * 说  明:封装函数，显示12*12点阵GB2312AscII到屏幕
 *
 * 入口参数：x,y	->显示坐标	asc	->要显示的AscII
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-23 16:13	基本功能完成
 ******************************************************************************/
FAR void GamAsciiS(U8 x,U8 y,U8 asc)
{
    c_VisScr=(U8 *)NULL;
    GamAscii(x,y,asc);
}
/*
 */

FAR	void	GamePictureDummy(U8 sX,U8 sY,U8 eX,U8 eY,U8* pic,U8* scr,U8 flag)
{
    int wid = eX - sX + 1;
    int hgt = eY - sY + 1;
    int x, y, X, Y;
    int scrPerLine = SCR_LINE;
    Rect scrRect = MakeRect(0, 0, SCR_WID, SCR_HGT);

    {
        int picPerLine = (wid + 7) / 8;
        for (y = 0; y < hgt; y++) {
            Y = sY + y;
            for (x = 0; x < wid; x++) {
                X = sX + x;
                if (!touchIsPointInRect(X, Y, scrRect)) {
                    continue;
                }
                unsigned char pixel, pixel1;
                int ind = scrPerLine * Y + X/8;

                if (flag == 4) {
                    pixel1 = 0;
                }
                else {

                    if (pic) {
                        pixel = pic[picPerLine*y + x/8] & (128 >> (x%8));
                    }
                    else {
                        pixel = 0;
                    }

                    {
                        pixel1 = scr[ind] & (128 >> (X%8));
                        
                        switch (flag) {
                            case 0: // Normal
                                pixel1 = pixel;
                                break;
                            case 1: // &
                                pixel1 = pixel && pixel1;
                                break;
                            case 2: // |
                                pixel1 = pixel || pixel1;
                                break;
                            case 4: // clear
                                break;
                            default:
                                break;
                        }
                    }
                }
                
                {
                    int mask = 1 << (7 - X%8);
                    pixel1 = pixel1 ? mask : 0;
                    scr[ind] = (scr[ind] & (~mask)) | pixel1;
                }
            }
        }
    }
}
