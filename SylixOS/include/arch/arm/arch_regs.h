/*********************************************************************************************************
**
**                                    中国软件开源组织
**
**                                   嵌入式实时操作系统
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------文件信息--------------------------------------------------------------------------------
**
** 文   件   名: arch_regs.h
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2013 年 05 月 04 日
**
** 描        述: ARM 寄存器相关.
*********************************************************************************************************/

#ifndef __ARM_ARCH_REGS_H
#define __ARM_ARCH_REGS_H

/*********************************************************************************************************
  arm cpsr
*********************************************************************************************************/

#define ARCH_ARM_USR32MODE          0x10                                /*  用户状态                    */
#define ARCH_ARM_FIQ32MODE          0x11                                /*  快速中断状态                */
#define ARCH_ARM_IRQ32MODE          0x12                                /*  中断状态                    */
#define	ARCH_ARM_SVC32MODE          0x13                                /*  管理状态                    */
#define ARCH_ARM_ABT32MODE          0x17                                /*  中止状态                    */
#define ARCH_ARM_UND32MODE          0x1B                                /*  未定义状态                  */
#define ARCH_ARM_SYS32MODE          0x1F                                /*  系统状态                    */
#define ARCH_ARM_DIS_FIQ            0x40                                /*  关闭 FIQ 中断               */
#define ARCH_ARM_DIS_IRQ            0x80                                /*  关闭 IRQ 中断               */

/*********************************************************************************************************
  寄存器表
*********************************************************************************************************/

typedef UINT        ARCH_REG_T;

typedef struct {
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    ARCH_REG_T      REG_uiPRIMASK;
    ARCH_REG_T      REG_uiR4;
    ARCH_REG_T      REG_uiR5;
    ARCH_REG_T      REG_uiR6;
    ARCH_REG_T      REG_uiR7;
    ARCH_REG_T      REG_uiR8;
    ARCH_REG_T      REG_uiR9;
    ARCH_REG_T      REG_uiR10;
    ARCH_REG_T      REG_uiR11;
    ARCH_REG_T      REG_uiExcRet;

    ARCH_REG_T      REG_uiR0;
    ARCH_REG_T      REG_uiR1;
    ARCH_REG_T      REG_uiR2;
    ARCH_REG_T      REG_uiR3;
    ARCH_REG_T      REG_uiR12;
    ARCH_REG_T      REG_uiR14;
    ARCH_REG_T      REG_uiR15;
    ARCH_REG_T      REG_uiXpsr;

#define REG_uiFp            REG_uiR11
#define REG_uiIp            REG_uiR12
#define REG_uiLr            REG_uiR14
#define REG_uiPc            REG_uiR15
#define REG_uiCpsr          REG_uiXpsr
#else
    ARCH_REG_T      REG_uiCpsr;
    ARCH_REG_T      REG_uiR0;
    ARCH_REG_T      REG_uiR1;
    ARCH_REG_T      REG_uiR2;
    ARCH_REG_T      REG_uiR3;
    ARCH_REG_T      REG_uiR4;
    ARCH_REG_T      REG_uiR5;
    ARCH_REG_T      REG_uiR6;
    ARCH_REG_T      REG_uiR7;
    ARCH_REG_T      REG_uiR8;
    ARCH_REG_T      REG_uiR9;
    ARCH_REG_T      REG_uiR10;
    ARCH_REG_T      REG_uiFp;
    ARCH_REG_T      REG_uiIp;
    ARCH_REG_T      REG_uiLr;
    ARCH_REG_T      REG_uiPc;
#endif                                  /*  defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)       */
} ARCH_REG_CTX;

/*********************************************************************************************************
  EABI 标准调用回溯堆栈表
*********************************************************************************************************/

typedef struct {
    ARCH_REG_T      FP_uiFp;
    ARCH_REG_T      FP_uiLr;
} ARCH_FP_CTX;

/*********************************************************************************************************
  从上下文中获取信息
*********************************************************************************************************/

#define ARCH_REG_CTX_GET_PC(ctx)    ((void *)(ctx).REG_uiPc)
#define ARCH_REG_CTX_GET_FRAME(ctx) ((void *)(ctx).REG_uiFp)
#define ARCH_REG_CTX_GET_STACK(ctx) ((void *)&(ctx))                    /*  不准确, 仅为了兼容性设计    */

/*********************************************************************************************************
  堆栈中的寄存器信息所占大小
*********************************************************************************************************/

#define ARCH_REG_CTX_WORD_SIZE  18
#define ARCH_STK_MIN_WORD_SIZE  128

#endif                                                                  /*  __ARM_ARCH_REGS_H           */
/*********************************************************************************************************
  END
*********************************************************************************************************/
