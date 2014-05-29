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
** 文   件   名: EventSetTryGet.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2006 年 12 月 20 日
**
** 描        述: 无阻塞等待事件集相关事件   (采用 uCOS 机制，加入链式反应能力)

** BUG
2008.01.13  加入 _DebugHandle() 功能。
2009.04.08  加入对 SMP 多核的支持.
2012.03.20  减少对 _K_ptcbTCBCur 的引用, 尽量采用局部变量, 减少对当前 CPU ID 获取的次数.
2013.07.18  使用新的获取 TCB 的方法, 确保 SMP 系统安全.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
/*********************************************************************************************************
             ERROR MACRO
*********************************************************************************************************/
#define  __EVENTSET_NOT_READY() do {                          \
             LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock,      \
                                    iregInterLevel);          \
             __KERNEL_EXIT();                                 \
             _ErrorHandle(ERROR_THREAD_WAIT_TIMEOUT);         \
             return (ERROR_THREAD_WAIT_TIMEOUT);              \
         } while (0)
/*********************************************************************************************************
** 函数名称: API_EventSetTryGet
** 功能描述: 无阻塞等待事件集相关事件
** 输　入  : 
**           ulId            事件集句柄
**           ulEvent         等待事件
**           ulOption        等待方法选项
** 输　出  : 事件句柄
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
#if (LW_CFG_EVENTSET_EN > 0) && (LW_CFG_MAX_EVENTSETS > 0)

LW_API  
ULONG  API_EventSetTryGet (LW_OBJECT_HANDLE  ulId, 
                           ULONG             ulEvent,
                           ULONG             ulOption)
{
             INTREG                iregInterLevel;
             
             PLW_CLASS_TCB         ptcbCur;
    REGISTER UINT16                usIndex;
    REGISTER PLW_CLASS_EVENTSET    pes;
    REGISTER BOOL                  bIsReset;
    REGISTER UINT8                 ucWaitType;
    REGISTER ULONG                 ulEventRdy;
    
    usIndex = _ObjectGetIndex(ulId);
    
    if (ulOption & LW_OPTION_EVENTSET_RESET) {                          /*  是否需要复位相关位          */
        bIsReset = LW_TRUE;
    } else {
        bIsReset = LW_FALSE;
    }
    
    ucWaitType = (UINT8)(ulOption & 0x0F);                              /*  获得等待类型                */
    
    LW_TCB_GET_CUR_SAFE(ptcbCur);                                       /*  当前任务控制块              */
    
#if LW_CFG_ARG_CHK_EN > 0
    if (!_ObjectClassOK(ulId, _OBJECT_EVENT_SET)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "eventset handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
    if (_EventSet_Index_Invalid(usIndex)) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "eventset handle invalidate.\r\n");
        _ErrorHandle(ERROR_KERNEL_HANDLE_NULL);
        return  (ERROR_KERNEL_HANDLE_NULL);
    }
#endif
    pes = &_K_esBuffer[usIndex];
    
    LW_SPIN_LOCK_QUICK(&pes->EVENTSET_slLock, &iregInterLevel);         /*  关闭中断同时锁住 spinlock   */
    
    if (_EventSet_Type_Invalid(usIndex, LW_TYPE_EVENT_EVENTSET)) {
        LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        _DebugHandle(__ERRORMESSAGE_LEVEL, "eventset handle invalidate.\r\n");
        _ErrorHandle(ERROR_EVENTSET_TYPE);
        return  (ERROR_EVENTSET_TYPE);
    }
    
    __KERNEL_ENTER();                                                   /*  进入内核                    */
    
    switch (ucWaitType) {
    
    case LW_OPTION_EVENTSET_WAIT_SET_ALL:
        ulEventRdy = (ulEvent & pes->EVENTSET_ulEventSets);
        if (ulEvent == ulEventRdy) {
            if (bIsReset) {                                             /*  消耗掉相关位                */
                pes->EVENTSET_ulEventSets &= (~ulEventRdy);
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
                API_EventSetSet(ulId, 0, LW_OPTION_EVENTSET_SET);       /*  可能还有其他任务被激活      */
            } else {
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
            }
            return  (ERROR_NONE);
        } else {
            __EVENTSET_NOT_READY();
        }
        break;
        
    case LW_OPTION_EVENTSET_WAIT_SET_ANY:
        ulEventRdy = (ulEvent & pes->EVENTSET_ulEventSets);
        if (ulEventRdy) {
            if (bIsReset) {
                pes->EVENTSET_ulEventSets &= (~ulEventRdy);
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
                API_EventSetSet(ulId, 0, LW_OPTION_EVENTSET_SET);       /*  可能还有其他任务被激活      */
            } else {
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
            }
            return  (ERROR_NONE);
        } else {
            __EVENTSET_NOT_READY();
        }
        break;
        
    case LW_OPTION_EVENTSET_WAIT_CLR_ALL:
        ulEventRdy = (ulEvent & ~pes->EVENTSET_ulEventSets);
        if (ulEvent == ulEventRdy) {
            if (bIsReset) {
                pes->EVENTSET_ulEventSets |= ulEventRdy;                /*  “清除”相关位              */
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
                API_EventSetSet(ulId, 0, LW_OPTION_EVENTSET_SET);       /*  可能还有其他任务被激活      */
            } else {
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
            }
            return  (ERROR_NONE);
        } else {
            __EVENTSET_NOT_READY();
        }
        break;
        
    case LW_OPTION_EVENTSET_WAIT_CLR_ANY:
        ulEventRdy = (ulEvent & ~pes->EVENTSET_ulEventSets);
        if (ulEventRdy) {
            if (bIsReset) {
                pes->EVENTSET_ulEventSets |= ulEventRdy;                /*  “清除”相关位              */
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
                API_EventSetSet(ulId, 0, LW_OPTION_EVENTSET_SET);       /*  可能还有其他任务被激活      */
            } else {
                ptcbCur->TCB_ulEventSets = ulEventRdy;
                LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, 
                                       iregInterLevel);                 /*  打开中断, 同时打开 spinlock */
                __KERNEL_EXIT();                                        /*  退出内核                    */
            }
            return  (ERROR_NONE);
        } else {
            __EVENTSET_NOT_READY();
        }
        break;
        
    default:
        LW_SPIN_UNLOCK_QUICK(&pes->EVENTSET_slLock, iregInterLevel);    /*  打开中断, 同时打开 spinlock */
        __KERNEL_EXIT();                                                /*  退出内核                    */
        _ErrorHandle(ERROR_EVENTSET_WAIT_TYPE);
        return  (ERROR_EVENTSET_WAIT_TYPE);
    }
}

#endif                                                                  /*  (LW_CFG_EVENTSET_EN > 0)    */
                                                                        /*  (LW_CFG_MAX_EVENTSETS > 0)  */
/*********************************************************************************************************
  END
*********************************************************************************************************/
