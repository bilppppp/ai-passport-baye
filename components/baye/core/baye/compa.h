#ifndef	_COMPA_H
#define	_COMPA_H
#include "../inc/dictsys.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "consdef.h"
/*+++++++++++++++
 通用函数的宏替换
 +++++++++++++++*/
/* 随机函数 */
//#define	gam_srand(seed)		srand(seed)		/* 替换A系列系统中的随机初始化函数 */
//#define	gam_rand()		_gam_rand()			/* 替换A系列系统中的随机函数 */
int gam_rand(void);
void gam_srand(unsigned int seed);
int gam_seed(void);

/* 内存函数 */
#define	gam_memcpy(a,s,l)	memcpy(a,s,l)			/* 替换A系列系统中常驻的内存拷贝函数 */
#define	gam_memset(buf,val,len)	memset(buf,val,len)			/* 替换A系列系统中的内存填充函数 */
#define	gam_malloc(len)		calloc(1, len)			/* 替换A系列系统中堆申请函数 */
#define	gam_free(ptr)		free(ptr)				/* 替换A系列系统中堆释放函数 */
/* 字符串函数 */
#define	gam_strcmp(str1,str2)	strcmp((const char*)str1,(const char*)str2)
#define	gam_strcat(str1,str2)	strcat((char*)str1,(const char*)str2)
#define	gam_strlen(str)		strlen((const char*)str)
#define	gam_itoa(i,str,bdh)	itoa(i,(char*)str,bdh)
#define	gam_ltoa(l,str,bdh)	ltoa(l,(char*)str,bdh)
#define	gam_atoi(str)		atoi((const char*)str)
#define	gam_atol(str)		atol((const char*)str)
/* 屏幕操作函数 */
#define	gam_clrlcd(l,t,r,b)	SysLcdPartClear(l,t,r,b)		/* 清除屏幕矩形 */
#define	gam_clrvscr(l,t,r,b,v)	GamePictureDummy(l,t,r,b,(U8*)NULL,v,4)	/* 清空虚拟屏幕指定区域 */
#define	gam_clslcd()		SysLcdPartClear(0,0,SCR_WID-1,SCR_HGT-1)/* 清除整个屏幕 */
#define	gam_revlcd(l,t,r,b)	SysLcdReverse(l,t,r,b)			/* 反显屏幕 */
#define	gam_putpixel(x,y,c)	SysPutPixel(x,y,c)			/* 画点函数 */
#define	gam_line(l,t,r,b)	SysLine(l,t,r,b)			/* 显示直线 */
#define	gam_linec(l,t,r,b)	SysLineClear(l,t,r,b)			/* 消隐直线 */
#define	gam_rect(l,t,r,b)	SysRect(l,t,r,b)			/* 显示矩形 */
#define	gam_rectc(l,t,r,b)	SysRectClear(l,t,r,b)			/* 消隐矩形 */
#define	gam_getkey()		SysGetKey()				/* 有按键获取，无按键返回 */
#define	gam_Picture(l,t,r,b,p)	SysPicture(l,t,r,b,p,0)	

#define gam_savscr() SysSaveScreen()
#define gam_restorescr() SysRestoreScreen()

#ifndef HAVE_ITOA
#include "itoa.h"
#endif

FAR void logPicture(U8 wid,U8 hgt, U8* buffer);

#endif	/* _COMPA_H */
