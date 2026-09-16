/***********************************************************************
 *Copyright (c)2005 , 东莞步步高教育电子分公司
 *All rights reserved.
 **
 文件名称：	citycmdd.c
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
 *	1.0    2005-7-12 11:17  陈泽伟     基本的功能完成
 ***********************************************************************/

#include "baye/stdsys.h"
#include "baye/comm.h"
#undef	CITYCMDD_C
#define	CITYCMDD_C
#include "baye/enghead.h"
#include "baye/bind-objects.h"

/******************************************************************************
 * 函数名:BattleMake
 * 说  明:“出征”命令生成
 *
 * 入口参数：所在城市编号
 *
 * 出口参数：1 -- 成功，0 -- 失败
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-3 17:31	基本功能完成
 ******************************************************************************/
FAR U8 BattleMake(U8 city)
{
    U8 fpptr[10];
    U8 *pqptr;
    U8 *str;
    U8 pcount;
    U8 pcode = 0;
    U8 ocity;
    U8 odis;
    U8 xs,ys;
    U8 i;
    OrderType order;

    if (!IsMoney(city,BATTLE))
    {
        /*金钱不足*/
        ShowConstStrMsg(NOTE_STR8);
        return(1);
    }

    if (g_engineConfig.fixFoodOverFlow && g_Cities[city].Food == 0)
    {
        /*粮草不足*/
        GamMsgBox((const U8*)"\xc1\xb8\xb2\xdd\xb2\xbb\xd7\xe3", 2);
        return(1);
    }

    xs = g_CityPos.setx;
    ys = g_CityPos.sety;

    pqptr = SHARE_MEM;

    pcount = GetCityPersons(city,pqptr);
    if (!pcount)
    {
        ShowConstStrMsg(STR_NOFIGHTER);
        return(1);
    }
    gam_memset(fpptr,0,10);
    for (i = 0;i < 10;i ++)
    {
        /*gam_clrlcd(WK_SX,WK_SY,WK_EX,WK_EY);*/
        ShowMapClear();
        if (pcode >= pcount) {
            pcode = pcount-1;
        }
        pcode = ShowPersonControl(pqptr,pcount,pcode,WK_SX + 4,WK_SY + 2,WK_EX - 4,WK_EY - 2);
        if (0xff != pcode)
        {
            fpptr[i] = pqptr[pcode] + 1;
            DelPerson(city,pqptr[pcode]);
        }
        else
        {
            break;
        }
        pcount = GetCityPersons(city,pqptr);
    }
    if (i)
    {
        /*gam_clrlcd(WK_SX,WK_SY,WK_EX,WK_EY);*/
        ShowMapClear();
        order.Food = GetFood(1,g_Cities[city].Food);
        if (0xffff == order.Food)
        {
            for (i --;(U8)(i + 1) >= 1;i --)
            {
                AddPerson(city,fpptr[i] - 1);
            }
        }
        else
        {

            while (1)
            {
                str = SHARE_MEM;
                ResLoadToMem(STRING_CONST,STR_OBJ,str);
                ShowMapClear();
                ShowGReport(fpptr[0] - 1,str);
                ocity = GetCitySet(&g_CityPos);
                if (0xff == ocity)
                {
                    for (i --;(U8)(i + 1) >= 1;i --)
                    {
                        AddPerson(city,fpptr[i] - 1);
                    }
                    break;
                }
                else if (ocity != city)
                {
                    if (g_Cities[ocity].Belong == g_Cities[city].Belong)
                    {
                        /*提示我方城池*/
                        ShowConstStrMsg(NOTE_STR7);
                        continue;
                    }
                    else
                    {
                        odis = AttackCityRoad(city,xs,ys,ocity,g_CityPos.setx,g_CityPos.sety);
                        if (0xff != odis)
                        {
                            ShowConstStrMsg(STR_ARMOUT);
                            if (g_engineConfig.fixOverFlow16) {
                                ADD16(g_Cities[city].Food, -order.Food);
                            } else {
                                g_Cities[city].Food -= order.Food;
                            }
                            OrderConsumeMoney(city,BATTLE);
                            order.OrderId = BATTLE;
                            order.City = city;
                            order.Object = ocity;
                            order.TimeCount = odis;
                            if (!AddFightOrder(&order,fpptr)) {
                                for (i --;(U8)(i + 1) >= 1;i --)
                                {
                                    AddPerson(city,fpptr[i] - 1);
                                }
                            }
                            break;
                        }
                        else
                        {
                            /*提示无法到达*/
                            ShowConstStrMsg(NOTE_STR4);
                            continue;
                        }
                    }
                }
            }
        }
    }

    return(1);
}

/******************************************************************************
 * 函数名:BattleDrv
 * 说  明:“出征”命令驱动
 *
 * 入口参数：Order -- 待执行命令
 *
 * 出口参数：1 -- 成功，0 -- 失败
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-3 17:31	基本功能完成
 ******************************************************************************/
FAR U8 BattleDrv(OrderType *Order)
{
    U8 *pqptr;
    U16 clen;
    U8 pcount,fcount,t;
    U8 pcode;
    U8 i;
    U8 *ptr,*fpptr,*midx;
    U8 o,ob,pb;

    pqptr = SHARE_MEM;
    midx = SHARE_MEM;
    fpptr = FIGHTERS_IDX;
    fpptr[Order->Person] = 0;
    fpptr = FIGHTERS;
    clen = 10;
    clen *= Order->Person;
    fpptr = FIGHTERS;
    fpptr = &fpptr[clen];
    ptr = g_FgtParam.GenArray;
    gam_memset(ptr,0,20);
    o = Order->Object;
    ob = g_Cities[o].Belong;
    pb = g_Persons[fpptr[0] - 1].Belong;

    ResItemGet(IFACE_CONID,dCityMapId,midx);
    g_FgtParam.MapId = FIGHT_MAP + midx[o];
    g_FgtParam.Way = GetDirect(o,Order->City);
    g_FgtParam.CityIndex = o;

    do
    {
        ShowAttackNote(pb - 1,o);
        if (ob == (g_PlayerKing + 1))
        {
            if (pb == (g_PlayerKing + 1))
            {
                /*城池已被我军战领*/
                for (i = 0;i < 10;i ++)
                {
                    if (fpptr[i])
                        AddPerson(o,fpptr[i] - 1);
                }
                ShowFightWinNote(pb - 1);
                break;
            }
            /*添加进入战斗代码*/
            /*ShowAttackMsg(pb - 1,o);*/
            g_FgtParam.Mode = FGT_DF;
            g_FgtParam.MProvender = g_Cities[o].Food;
            g_FgtParam.EProvender = Order->Food;
            for (i = 0;i < 10;i ++)
            {
                pcount = GetCityPersons(o,pqptr);
                /*gam_clrlcd(WK_SX,WK_SY,WK_EX,WK_EY);*/
                ShowMapClear();
                pcode = ShowPersonControl(pqptr,pcount,0,WK_SX + 4,WK_SY + 2,WK_EX - 4,WK_EY - 2);
                if (0xff != pcode)
                {
                    ptr[i] = pqptr[pcode] + 1;
                    DelPerson(o,pqptr[pcode]);
                }
                else
                {
                    break;
                }
            }
            gam_memcpy(&ptr[10],fpptr,10);
            GamFight();
        }
        else if (pb == (g_PlayerKing + 1))
        {
            if (!ob)
            {
                for (i = 0;i < 10;i ++)
                {
                    if (fpptr[i])
                        AddPerson(o,fpptr[i] - 1);
                }
                g_Cities[o].Belong = pb;
                ShowFightWinNote(pb - 1);
                break;
            }

            if (ob == (g_PlayerKing + 1))
            {
                /*城池已被我军战领*/
                for (i = 0;i < 10;i ++)
                {
                    if (fpptr[i])
                        AddPerson(o,fpptr[i] - 1);
                    ShowFightWinNote(pb - 1);
                }
                break;
            }
            /*添加进入战斗代码*/
            g_FgtParam.Mode = FGT_AT;
            g_FgtParam.MProvender = Order->Food;
            g_FgtParam.EProvender = g_Cities[o].Food;
            gam_memcpy(ptr,fpptr,10);

            pcount = GetCityPersons(o,pqptr);
            if (pcount)
            {
                fcount = 0;
                for (fcount = 0;fcount < pcount - 1;fcount ++)
                {
                    for (i = fcount + 1;i < pcount;i ++)
                    {
                        /*添加武将兵力排序*/
                        if (g_Persons[pqptr[fcount]].Arms < g_Persons[pqptr[i]].Arms)
                        {
                            t = pqptr[i];
                            pqptr[i] = pqptr[fcount];
                            pqptr[fcount] = t;
                        }
                        if (!g_Persons[pqptr[pcount - 1]].Arms)
                            pcount -= 1;
                    }
                }

                if (!pcount)
                {
                    /*没有可出征武将*/
                    ptr[10] = pqptr[0] + 1;
                    DelPerson(o,pqptr[0]);
                }
                else
                {
                    /*添加出征命今生成代码*/
                    if (pcount > 10)
                    {
                        pcount = 10;
                    }
                    for (i = 0;i < pcount;i ++)
                    {
                        DelPerson(o,pqptr[i]);
                        ptr[10 + i] = pqptr[i] + 1;
                    }
                }
            }
            /*ShowAttackMsg(g_PlayerKing,o);*/

            GamFight();

        }
        else
        {
            if (!ob)
            {
                for (i = 0;i < 10;i ++)
                {
                    if (fpptr[i])
                        AddPerson(o,fpptr[i] - 1);
                }
                g_Cities[o].Belong = pb;
                ShowFightWinNote(pb - 1);
                break;
            }

            if (ob == pb)
            {
                for (i = 0;i < 10;i ++)
                {
                    if (fpptr[i])
                        AddPerson(o,fpptr[i] - 1);
                }
                ShowFightWinNote(pb - 1);
                break;
            }
            gam_memcpy(ptr,fpptr,10);

            pcount = GetCityPersons(o,pqptr);
            if (pcount)
            {
                fcount = 0;
                for (fcount = 0;fcount < pcount - 1;fcount ++)
                {
                    for (i = fcount + 1;i < pcount;i ++)
                    {
                        /*添加武将兵力排序*/
                        if (g_Persons[pqptr[fcount]].Arms < g_Persons[pqptr[i]].Arms)
                        {
                            t = pqptr[i];
                            pqptr[i] = pqptr[fcount];
                            pqptr[fcount] = t;
                        }
                        if (!g_Persons[pqptr[pcount - 1]].Arms)
                            pcount -= 1;
                    }
                }

                if (!pcount)
                {
                    /*没有可出征武将*/
                    ptr[10] = pqptr[0];
                    DelPerson(o,pqptr[0]);
                }
                else
                {
                    /*添加出征命今生成代码*/
                    if (pcount > 10)
                    {
                        pcount = 10;
                    }
                    for (i = 0;i < pcount;i ++)
                    {
                        DelPerson(o,pqptr[i]);
                        ptr[10 + i] = pqptr[i] + 1;
                    }
                }
            }
            /*添加战斗计算代码*/
            g_FgtParam.Mode = FGT_AUTO;
            g_FgtParam.MProvender = Order->Food;
            g_FgtParam.EProvender = g_Cities[o].Food;
            /*ShowAttackMsg(pb - 1,o);*/
            ShowFightNote(pb - 1,ob - 1);

            GamFight();

        }
        FightResultDeal(o,g_FgtOver);
    } while (0);


    return(1);
}

/******************************************************************************
 * 函数名:FightResultDeal
 * 说  明:战斗结束处理
 *
 * 入口参数：city -- 发生战斗城市，result -- 战斗结果
 *
 * 出口参数：0 -- 异常，1 -- 正常
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-7-26 20:53	基本功能完成
 ******************************************************************************/
FAR U8 FightResultDeal(U8 city,U8 result)
{
    U8 *str,*astr;
    U8 cking;
    U8 i;
    U8 *ptr;
    U8 *pqptr;
    CityType *cptr;

    ptr = g_FgtParam.GenArray;
    pqptr = SHARE_MEM;
    cking = 0xff;
    cavps = 0;
    switch (result)
    {
        case FGT_WON:
            for (i = 0;i < 10;i ++)
            {
                if (ptr[i])
                {
                    AddPerson(city,ptr[i] - 1);
                }
            }
            /*if (FGT_AUTO != g_FgtParam.Mode)*/
            ShowFightWinNote(g_Persons[ptr[0] - 1].Belong - 1);
            if (FGT_AT == g_FgtParam.Mode || FGT_AUTO == g_FgtParam.Mode)
            {
                cking = BeOccupied(ptr[0] - 1,city);

            }
            i = TheLoserDeal(city,&ptr[10]);
            if (0xff != i)
                cking = i;
            break;
        case FGT_LOSE:
            for (i = 10;i < 20;i ++)
            {
                if (ptr[i])
                {
                    AddPerson(city,ptr[i] - 1);
                }
            }
            if (FGT_DF == g_FgtParam.Mode)
            {
                cking = BeOccupied(ptr[10] - 1,city);
            }
            i = TheLoserDeal(city,ptr);
            if (0xff != i)
                cking = i;

            if (FGT_AUTO != g_FgtParam.Mode)
            {
                ShowFightLossNote();
                if (cavps)
                {
                    str = SHARE_MEM;
                    astr = SHARE_MEM + 400;
                    if (cavps > 1)
                    {
                        cavps = 2;
                    }
                    GetPersonName(cavpdb,str);
                    ResLoadToMem(STRING_CONST,STR_CAV_NOTE1 + cavps - 1,astr);
                    gam_strcat(str,astr);
                    GamMsgBox(str,2);
                }
            }
            else
            {
                ShowFightWinNote(g_Persons[ptr[10] - 1].Belong - 1);
            }

            break;
    }
    cptr = &g_Cities[city];
    cptr->Farming = cptr->Farming - cptr->Farming / 20;
    cptr->Commerce = cptr->Commerce - cptr->Commerce / 20;
    cptr->Money = cptr->Money - cptr->Money / 20;
    cptr->PeopleDevotion = cptr->PeopleDevotion - cptr->PeopleDevotion / 10;

    if (0xff != cking)
    {
        /*添加另立新君代码*/
        KingOverDeal(cking);
    }

    if (g_engineConfig.fixOverFlow16) {
        cptr->Food = add_16(g_FgtParam.MProvender, g_FgtParam.EProvender);
    } else {
        cptr->Food = g_FgtParam.MProvender + g_FgtParam.EProvender;
    }

    return(0);
}

/******************************************************************************
 * 函数名:TheLoserDeal
 * 说  明:战斗失败方处理
 *
 * 入口参数：city -- 发生城市，lqueue -- 战败方武将队列
 *
 * 出口参数：0xff -- 无特殊，其他值 -- 遭劫君主编号
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-8-19 15:43	基本功能完成
 ******************************************************************************/
U8 TheLoserDeal(U8 city,U8 *lqueue)
{
    U8 *str,*astr;
    U8 i;
    U8 *gptr;
    U8 cking;
    U8 rnd;
    U8 p;
    PersonType *pptr;

    cking = 0xff;
    str = SHARE_MEM + 3000;
    astr = SHARE_MEM + 3400;
    for (i = 0;i < 10;i ++)
    {
        if (lqueue[i])
        {
            p = lqueue[i] - 1;
            pptr = &g_Persons[p];
            rnd = gam_rand() % 100;
            if (rnd > pptr->IQ)
            {
                if (pptr->Belong == lqueue[i])
                    cking = p;
                HoldCaptive(p,city);
            }
            else
            {
                /*添加逃跑代码*/
                if (!LostEscape(p,city))
                {
                    if (pptr->Belong == lqueue[i])
                        cking = p;

                    if (rnd)
                    {
                        HoldCaptive(p,city);
                        continue;
                    }

                    gptr = pptr->Equip;
                    if (gptr[0])
                    {
                        U8 index = AddGoods(city,gptr[0] - 1);
                        SetGoodsByIndex(index-1);
                    }
                    if (gptr[1])
                    {
                        U8 index = AddGoods(city,gptr[1] - 1);
                        SetGoodsByIndex(index-1);
                    }
                    if (FGT_AUTO != g_FgtParam.Mode)
                    {
                        GetPersonName(p,str);
                        ResLoadToMem(STRING_CONST,STR_PERSONOVER,astr);
                        gam_strcat(str,astr);
                        GamMsgBox(str,2);
                    }
                }
            }
        }
    }
    return(cking);
}

/******************************************************************************
 * 函数名:HoldCaptive
 * 说  明:俘虏处理
 *
 * 入口参数：person -- 被俘虏武将，city -- 俘虏到城市
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-7-27 9:32	基本功能完成
 ******************************************************************************/
void HoldCaptive(U8 person,U8 city)
{
    PersonType *pptr;

    pptr = &g_Persons[person];
    pptr->OldBelong = pptr->Belong;
    pptr->Belong = 0xff;
    pptr->Arms = 0;
    cavpdb = person;
    cavps += 1;
    AddPerson(city,person);
}

/******************************************************************************
 * 函数名:LostEscape
 * 说  明:逃跑处理
 *
 * 入口参数：person -- 逃跑武将，city -- 逃跑起始城市
 *
 * 出口参数：0 -- 逃跑失败，1 -- 逃跑成功
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-7-27 9:47	基本功能完成
 ******************************************************************************/
U8 LostEscape(U8 person,U8 city)
{
    U8 rnd;
    U8 pcount;
    U8 *cqptr;

    cqptr = SHARE_MEM;
    pcount = GetKingCitys(g_Persons[person].Belong - 1,cqptr);
    if (pcount)
    {
        rnd = gam_rand() % pcount;
        AddPerson(cqptr[rnd],person);
        return(1);
    }
    return(0);
}

/******************************************************************************
 * 函数名:BeOccupied
 * 说  明:沦陷处理
 *
 * 入口参数：person -- 进攻武将，city -- 沦陷城市
 *
 * 出口参数：0xff -- 正常，其他 -- 被俘虏君主编号
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-7-27 9:52	基本功能完成
 ******************************************************************************/
U8 BeOccupied(U8 person,U8 city)
{
    U8 i,pcount,rev;
    U8 p;
    U8 *pqptr;
    PersonType *pptr;

    pqptr = SHARE_MEM;
    pcount = GetCityPersons(city,pqptr);
    g_Cities[city].Belong = g_Persons[person].Belong;
    g_Cities[city].SatrapId = person + 1;
    rev = 0xff;
    for (i = 0;i < pcount;i ++)
    {
        p = pqptr[i];
        pptr = &g_Persons[p];

        if (pptr->Belong == (p + 1))
        {
            rev = p;
            cavpdb = p;
            cavps += 1;
            pptr->Belong = 0xff;
        }
        else
        {
            pptr->Belong = 0;
        }
        /*pptr->Id = pptr->Belong;*/
        pptr->Arms = 0;
    }
    return (rev);
}

/******************************************************************************
 * 函数名:KingOverDeal
 * 说  明:君主被俘虏或战死处理
 *
 * 入口参数：君主
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005/5/18 11:26AM	基本功能完成
 ******************************************************************************/
FAR void KingOverDeal(U8 king)
{
    U8 i;
    U8 *cqptr,*pqptr;
    U8 ccount,pcount,pcode;

    KingDeadNote(king);
    cqptr = SHARE_MEM;
    pqptr = cqptr + CITY_MAX;
    ccount = GetKingCitys(king,cqptr);
    if (ccount)
    {
        pcount = GetKingPersons(king,pqptr);
        if (pcount)
        {
            if (king == g_PlayerKing)
            {
                /*提示君主遭劫，拥立新君*/
                ShowConstStrMsg(STR_MAKENEWKING);
                do
                {
                    pcode = ShowPersonControl(pqptr,pcount,0,WK_SX + 4,WK_SY + 2,WK_EX - 4,WK_EY - 2);
                } while (0xff == pcode);
                g_PlayerKing = pqptr[pcode];
            }
            else
            {
                pcode = 0;
                for (i = 1;i < pcount;i ++)
                {
                    if (g_Persons[pqptr[pcode]].IQ < g_Persons[pqptr[i]].IQ)
                        pcode = i;
                }
            }
            NewKingNote(pqptr[pcode]);
            g_Persons[pqptr[pcode]].Devotion = 100;
            for (i = 0;i < ccount;i ++)
            {
                g_Cities[cqptr[i]].Belong = pqptr[pcode] + 1;
            }
            for (i = 0;i < pcount;i ++)
            {
                g_Persons[pqptr[i]].Belong = pqptr[pcode] + 1;
            }
        }
        else
        {
            /*提示势力灭亡*/
            WeightOverNote(king);
            for (i = 0;i < ccount;i ++)
            {
                g_Cities[cqptr[i]].Belong = 0;
            }
        }
    }
    else
    {
        /*提示势力灭亡*/
        WeightOverNote(king);
    }
}

/******************************************************************************
 * 函数名:KingDeadNote
 * 说  明:君主被俘虏或战死提示
 *
 * 入口参数：君主
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005/5/18 11:26AM	基本功能完成
 ******************************************************************************/
void KingDeadNote(U8 king)
{
    U8 *str,*sbuf;


    str = SHARE_MEM + 3000;
    sbuf = SHARE_MEM + 3400;
    ResLoadToMem(STRING_CONST,STR_KING,str);
    GetPersonName(king,sbuf);
    gam_strcat(str,sbuf);
    ResLoadToMem(STRING_CONST,STR_DEAD,sbuf);
    gam_strcat(str,sbuf);
    GamMsgBox(str,2);
}

/******************************************************************************
 * 函数名:NewKingNote
 * 说  明:新立君主提示
 *
 * 入口参数：君主
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005/5/18 11:26AM	基本功能完成
 ******************************************************************************/
FAR void NewKingNote(U8 king)
{
    U8 *str,*sbuf;
    
    str = SHARE_MEM + 3000;
    sbuf = SHARE_MEM + 3400;
    GetPersonName(king,str);
    ResLoadToMem(STRING_CONST,STR_BEKING,sbuf);
    gam_strcat(str,sbuf);
    GamMsgBox(str,2);
}

/******************************************************************************
 * 函数名:BeNewKingNote
 * 说  明:被策反新立君主提示
 *
 * 入口参数：君主
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005/5/18 11:26AM	基本功能完成
 ******************************************************************************/
FAR void BeNewKingNote(U8 king)
{
    U8 *str,*sbuf;
    
    str = SHARE_MEM + 3000;
    sbuf = SHARE_MEM + 3400;
    GetPersonName(king,str);
    ResLoadToMem(STRING_CONST,STR_BEKINGED,sbuf);
    gam_strcat(str,sbuf);
    GamMsgBox(str,2);
}

/******************************************************************************
 * 函数名:WeightOverNote
 * 说  明:势力灭亡提示
 *
 * 入口参数：君主
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005/5/18 11:26AM	基本功能完成
 ******************************************************************************/
FAR void WeightOverNote(U8 king)
{
    U8 *str,*sbuf;
    
    str = SHARE_MEM + 3000;
    sbuf = SHARE_MEM + 3400;
    GetPersonName(king,str);
    ResLoadToMem(STRING_CONST,STR_OVER,sbuf);
    gam_strcat(str,sbuf);
    GamMsgBox(str,2);
}

/******************************************************************************
 * 函数名:WonPersonNoet
 * 说  明:提示有加入我军
 *
 * 入口参数：加入将领
 *
 * 出口参数：无
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005/5/18 11:26AM	基本功能完成
 ******************************************************************************/
FAR void WonPersonNoet(U8 person)
{
    U8 *str;
    
    str = SHARE_MEM + 3000;
    ResLoadToMem(STRING_CONST,STR_WONP,str);
    ShowGReport(person,str);
}

static U8 canAddOrder(OrderType *Order) {
#define _ST OrderType
    static Field _fields[] = {
        _FIELD_RW(OrderId, U8),
        _FIELD_RW(Person, U8),
        _FIELD_RW(City, U8),
        _FIELD_RW(Object, U8),
        _FIELD_RW(Arms, U16),
        _FIELD_RW(Food, U16),
        _FIELD_RW(Money, U16),
        _FIELD_RW(Consume, U8),
        _FIELD_RW(TimeCount, U8),
    };

    static ObjectDef _obj_def = {
        AL(_fields), 0, sizeof(_ST), _fields
    };

    static ValueDef _value_def = {
        .type = ValueTypeObject,
        .size = sizeof(_ST),
        .subdef.objDef = &_obj_def,
    };
    static Value context = {.def = &_value_def };
#undef _ST

    context.offset = (U32)Order;
    if (call_hook_a("willAddOrder", &context) == 0){
        return 0;
    }
    return 1;
}

/******************************************************************************
 * 函数名:AddOrderHead
 * 说  明:添加命令到命令队列尾
 *
 * 入口参数：Order -- 要添加的命令
 *
 * 出口参数：1 -- 成功，0 -- 失败
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-4 8:51	基本功能完成
 ******************************************************************************/
FAR U8 AddOrderHead(OrderType *Order)
{
    U8 i;
    OrderType *inode;

    if(!canAddOrder(Order)) return 0;

    inode = (OrderType *) ORDERQUEUE;
    for (i = 0;i < ORDER_MAX;i ++)
    {
        if (0xff == inode[i].OrderId)
        {
            gam_memcpy((U8 *) &inode[i],(U8 *) Order,sizeof(OrderType));
            return(1);
        }
    }
    // 命令队列已满
    GamMsgBox((U8*)"\xc3\xfc\xc1\xee\xb6\xd3\xc1\xd0\xd2\xd1\xc2\xfa", 1);
    
    /*OrderQueueType *orderadd;
     orderadd = (OrderQueueType *) gam_malloc(sizeof(OrderQueueType));
     if (NULL == orderadd)
     {
     return(0);
     }
     gam_memcpy((U8 *) &(orderadd->iOrder),(U8 *) Order,sizeof(OrderType));
     
     orderadd->nOrder = g_OrderHead;
     g_OrderHead = orderadd;*/
    
    return(0);
}

/******************************************************************************
 * 函数名:AddOrderEnd
 * 说  明:添加命令到命令队列尾
 *
 * 入口参数：Order -- 要添加的命令
 *
 * 出口参数：1 -- 成功，0 -- 失败
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-4 8:51	基本功能完成
 ******************************************************************************/
FAR U8 AddOrderEnd(OrderType *Order)
{
    U8 i;
    OrderType *inode;

    if(!canAddOrder(Order)) return 0;
    
    inode = (OrderType *) ORDERQUEUE;
    for (i = ORDER_MAX - FIGHT_ORDER_MAX;i < ORDER_MAX;i ++)
    {
        if (0xff == inode[i].OrderId)
        {
            gam_memcpy((U8 *) &inode[i],(U8 *) Order,sizeof(OrderType));
            return(1);
        }
    }
    // 命令队列已满
    GamMsgBox((U8*)"\xc3\xfc\xc1\xee\xb6\xd3\xc1\xd0\xd2\xd1\xc2\xfa", 1);
    
    /*OrderQueueType *orderadd;
     orderadd = (OrderQueueType *) gam_malloc(sizeof(OrderQueueType));
     if (NULL == orderadd)
     {
     return(0);
     }
     gam_memcpy((U8 *) &(orderadd->iOrder),(U8 *) Order,sizeof(OrderType));
     
     if (NULL == g_OrderHead)
     {
     g_OrderHead = orderadd;
     }
     else
     {
     g_OrderEnd->nOrder = orderadd;
     }
     g_OrderEnd = orderadd;
     g_OrderEnd->nOrder = (OrderQueueType *) NULL;*/
    
    return(0);
}

/******************************************************************************
 * 函数名:AddFightOrder
 * 说  明:添加出征命令到命令队列
 *
 * 入口参数：Order -- 要添加的命令，Fighters -- 出征武将队列
 *
 * 出口参数：1 -- 成功，0 -- 失败
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-4 8:51	基本功能完成
 ******************************************************************************/
FAR U8 AddFightOrder(OrderType *Order,U8 *Fighters)
{
    U8 i;
    U8 *fiptr,*fptr;
    U16 clen;
    
    fiptr = FIGHTERS_IDX;
    for (i = 0;i < FIGHT_ORDER_MAX;i ++)
    {
        if (!fiptr[i])
        {
            Order->Person = i;
            if (AddOrderEnd(Order)) {
                fiptr[i] = 1;
                fptr = FIGHTERS;
                clen = 10;
                clen *= i;
                gam_memcpy(&fptr[clen],Fighters,10);
                return(1);
            }
        }
    }
    // 出征命令队列已满
    GamMsgBox((U8*)"\xb3\xf6\xd5\xf7\xc3\xfc\xc1\xee\xb6\xd3\xc1\xd0\xd2\xd1\xc2\xfa", 1);
    return(0);
}

/******************************************************************************
 * 函数名:DelOrder
 * 说  明:从命令队列中删除一个命令
 *
 * 入口参数：Order -- 要删除的命令队列节点
 *
 * 出口参数：1 -- 成功，0 -- 失败
 *
 * 修改历史:
 *		姓名		日期			说明
 *		----		----			-----------
 *		陈泽伟		2005-6-4 9:06	基本功能完成
 ******************************************************************************/
FAR U8 DelOrder(OrderType *Order)
{
    Order->OrderId = 0xff;
    /*U8 i;
     OrderQueueType *orderqueue;
     
     if (NULL == g_OrderHead)
     return(0);
     
     if (g_OrderHead == Order)
     {
     g_OrderHead = Order->nOrder;
     }
     else
     {
     orderqueue = g_OrderHead;
     while (1)
     {
     if (NULL == orderqueue)
     return(0);
     
     if (Order == orderqueue->nOrder)
     break;
     
     orderqueue = orderqueue->nOrder;
     }
     orderqueue->nOrder = Order->nOrder;
     }
     
     if (!gam_free((U8 *) Order))
     return(0);
     
     if (NULL == g_OrderHead)
     g_OrderEnd = (OrderQueueType *) NULL;
     else if (NULL == orderqueue->nOrder)
     g_OrderEnd = orderqueue;*/
    
    return(1);
}
