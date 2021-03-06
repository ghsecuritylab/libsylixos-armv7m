/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                       SylixOS(TM)
**
**                               Copyright  All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: symTableStatic.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2010 年 02 月 26 日
**
** 描        述: 系统提供的静态符号表.
*********************************************************************************************************/

#ifndef __SYMSTATIC_H
#define __SYMSTATIC_H

/*********************************************************************************************************
  系统静态符号表
*********************************************************************************************************/
#ifdef  __SYMBOL_MAIN_FILE
LW_SYMBOL   _G_symStatic[] = {
    /*
     *  NULL
     */
    {   {LW_NULL, LW_NULL},  
        "null", (caddr_t)0, 
        LW_SYMBOL_FLAG_STATIC | LW_SYMBOL_FLAG_REN | LW_SYMBOL_FLAG_XEN
    },
    
    /*
     *  GCC 没有将 __errno 加入 libsylixos.a (不懂了!)
     */
    {   {LW_NULL, LW_NULL},
        "__errno", (caddr_t)__errno,
        LW_SYMBOL_FLAG_STATIC | LW_SYMBOL_FLAG_REN | LW_SYMBOL_FLAG_XEN
    },
};
#else
extern LW_SYMBOL   _G_symStatic[];
#endif                                                                  /*  __SYMBOL_MAIN_FILE          */

#endif                                                                  /*  __SYMSTATIC_H               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
