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
** 文   件   名: lwip_fix.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 05 月 06 日
**
** 描        述: lwip SylixOS 移植. 注意: 所有移植层的函数只能用于 lwip 内部使用!

** BUG:
2009.05.20  网络线程应该具有安全属性.
2009.05.21  修正的短超时的错误.
2009.05.27  系统支持 read abort 操作.
2009.05.28  timeout 清除使用 ptcb 作为参数.
2009.06.04  当 sys_mbox_post 发生溢出时, 需要打印 log.
2009.06.19  在创建对象错误时, 需要打印 _DebugHandle().
2009.07.04  将 fprintf(stderr, ...); 一些致命错误直接用 panic 替代 (目前从来没有遇到).
2009.07.28  sio_open() 时文件等待时间初始化为无限长.
2009.08.26  加入统计信息处理.
2009.08.29  加入 sio_tryread() 以支持 lwip 1.4.0
2009.09.04  使用二值信号量, 计数信号量可能造成问题!
            lwip 应用层使用消息传递, 将需要处理的函数通过消息的方式传递到 tcpip 任务, 然后等待完成(信号量)
            但是某些情况可能是信号量有残余, 直接就会返回, 因为消息是局部变量, 所以 tcpip 优先级低于应用时,
            收到消息时, 消息已经完全错误...
2009.10.29  sys_now 毫秒数计算错误.
2009.11.23  timeout 控制块改用 lwip 内存分配获取.
2010.01.04  lwip 1.4.0 以上版本实现了自己的 timeout 链, 不再需要为每个线程提供 timeouts.
2010.02.22  升级 lwip 实现新的 mutex 机制和新的参数.
2010.04.12  加入统一的 ip_input_hook() 函数.
2011.06.18  sys_now() 直接使用 64 位系统时间.
2011.06.23  加入 aodv_ip_input_hook() 处理.
2012.08.23  加入 ppp_link_status_hook(), ppp 拨号器可直接使用此函数作为状态回调.
2012.09.28  sys_mbox_post() 如果遇到 mbox 已满, 等待间隔改为 10ms.
            sys_mbox_post() 增加一种新的方式, 即可允许写阻塞.
2013.04.23  sys_thread_new() 修正注释.
2013.09.04  优化代码顺序并加入 lwip 断言专用的输出函数.
2014.07.01  SIO 驱动所有的文件描述符为内核文件描述符操作.
*********************************************************************************************************/
#define  __SYLIXOS_STDIO
#define  __SYLIXOS_PANIC
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0
#include "lwip_config.h"
#include "lwip_fix.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/sio.h"
#include "lwip/netdb.h"
#include "lwip/stats.h"
#include "lwip/init.h"
#include "lwip/pbuf.h"
#include "netif/aodvif.h"
#if PPP_SUPPORT > 0 || PPPOE_SUPPORT > 0
#include "net/if.h"
#include "lwip/pppapi.h"
#endif                                                                  /*  PPP_SUPPORT > 0 ||          */
                                                                        /*  PPPOE_SUPPORT > 0           */
/*********************************************************************************************************
  版本判断 (警告!!! 低版本 lwip 系统将不再支持)
*********************************************************************************************************/
#if (LWIP_VERSION_MAJOR >= 1 && LWIP_VERSION_MINOR < 4) ||  \
    (LWIP_VERSION_MAJOR < 1)
#error lwip version too old!
#endif                                                                  /*  LWIP 1.4.0 以下版本         */
/*********************************************************************************************************
  内部操作宏
  由于 SylixOS 消息队列发送函数无法实现写阻塞, 则这里利用消息队列未使用的 EVENT_pvTcbOwn 保存一个信号量
  作为写阻塞使用. (SylixOS 内部 EVENT_pvTcbOwn 只被互斥信号量使用)
*********************************************************************************************************/
#define __LW_MSG_QUEUE_PRIVAT(ulId)     (_K_eventBuffer[_ObjectGetIndex(ulId)].EVENT_pvTcbOwn)
/*********************************************************************************************************
  全局变量
*********************************************************************************************************/
spinlock_t   _G_slLwip;                                                 /*  多核自旋锁                  */
/*********************************************************************************************************
** 函数名称: sys_init
** 功能描述: 系统接口初始化
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_init (void)
{
    LW_SPIN_INIT(&_G_slLwip);                                           /*  初始化网络关键区域自旋锁    */
}
/*********************************************************************************************************
** 函数名称: sys_arch_protect
** 功能描述: 系统接口 SYS_ARCH_PROTECT
** 输　入  : pireg     中断等级状态
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_arch_protect (INTREG  *pireg)
{
    LW_SPIN_LOCK_QUICK(&_G_slLwip, pireg);
}
/*********************************************************************************************************
** 函数名称: sys_arch_unprotect
** 功能描述: 系统接口 SYS_ARCH_UNPROTECT
** 输　入  : ireg      中断等级状态
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_arch_unprotect (INTREG  ireg)
{
    LW_SPIN_UNLOCK_QUICK(&_G_slLwip, ireg);
}
/*********************************************************************************************************
** 函数名称: sys_assert_print
** 功能描述: 系统断言失败打印
** 输　入  : msg       失败信息
**           func      所属函数
**           file      所属文件
**           line      所在行标
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_assert_print (const char *msg, const char *func, const char *file, int line)
{
    fprintf(stderr, "lwip assert: %s func: %s file: %s line: %d\n", msg, func, file, line);
}
/*********************************************************************************************************
** 函数名称: sys_error_print
** 功能描述: 系统错误打印
** 输　入  : msg       失败信息
**           func      所属函数
**           file      所属文件
**           line      所在行标
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_error_print (const char *msg, const char *func, const char *file, int line)
{
    fprintf(stderr, "lwip error: %s func: %s file: %s line: %d\n", msg, func, file, line);
}
/*********************************************************************************************************
** 函数名称: sys_mutex_new
** 功能描述: 创建一个 lwip 互斥量
** 输　入  : pmutex    创建的互斥量
** 输　出  : 错误码
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
err_t  sys_mutex_new (sys_mutex_t *pmutex)
{
    SYS_ARCH_DECL_PROTECT(lev);
    LW_OBJECT_HANDLE    hMutex = API_SemaphoreMCreate("lwip_mutex", LW_PRIO_DEF_CEILING, 
                                                      LW_OPTION_INHERIT_PRIORITY |
                                                      LW_OPTION_DELETE_SAFE |
                                                      LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (hMutex == LW_OBJECT_HANDLE_INVALID) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not create lwip mutex.\r\n");
        SYS_STATS_INC(mutex.err);
        return  (ERR_MEM);
    
    } else {
        if (pmutex) {
            *pmutex = hMutex;
        }
        SYS_STATS_INC(mutex.used);
        
        SYS_ARCH_PROTECT(lev);
        if (lwip_stats.sys.mutex.used > lwip_stats.sys.mutex.max) {
            lwip_stats.sys.mutex.max = lwip_stats.sys.mutex.used;
        }
        SYS_ARCH_UNPROTECT(lev);
        return  (ERR_OK);
    }
}
/*********************************************************************************************************
** 函数名称: sys_mutex_free
** 功能描述: 删除一个 lwip 互斥量
** 输　入  : pmutex      互斥量
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mutex_free (sys_mutex_t *pmutex)
{
    API_SemaphoreMDelete(pmutex);
    SYS_STATS_DEC(mutex.used);
}
/*********************************************************************************************************
** 函数名称: sys_mutex_lock
** 功能描述: 锁定一个 lwip 互斥量
** 输　入  : pmutex      互斥量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mutex_lock (sys_mutex_t  *pmutex)
{
    if (pmutex) {
        API_SemaphoreMPend(*pmutex, LW_OPTION_WAIT_INFINITE);
    }
}
/*********************************************************************************************************
** 函数名称: sys_mutex_unlock
** 功能描述: 解锁一个 lwip 互斥量
** 输　入  : pmutex      互斥量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mutex_unlock (sys_mutex_t  *pmutex)
{
    if (pmutex) {
        API_SemaphoreMPost(*pmutex);
    }
}
/*********************************************************************************************************
** 函数名称: sys_mutex_valid
** 功能描述: 检查一个 lwip 互斥量是否有效
** 输　入  : pmutex      互斥量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
int  sys_mutex_valid (sys_mutex_t  *pmutex)
{
    if (pmutex) {
        if (*pmutex) {
            return  (1);
        }
    }
    
    return  (0);
}
/*********************************************************************************************************
** 函数名称: sys_mutex_set_invalid
** 功能描述: 将一个 lwip 互斥量设置为无效
** 输　入  : pmutex      互斥量
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mutex_set_invalid (sys_mutex_t *pmutex)
{
    if (pmutex) {
        *pmutex = SYS_MUTEX_NULL;
    }
}
/*********************************************************************************************************
** 函数名称: sys_sem_new
** 功能描述: 创建一个 lwip 信号量
** 输　入  : psem      创建的信号量
**           count     初始计数值
** 输　出  : 错误码
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
err_t  sys_sem_new (sys_sem_t  *psem, u8_t  count)
{
    SYS_ARCH_DECL_PROTECT(lev);
    LW_OBJECT_HANDLE    hSemaphore = API_SemaphoreCCreate("lwip_sem", (ULONG)count, 0x1, 
                                                          LW_OPTION_WAIT_FIFO |
                                                          LW_OPTION_OBJECT_GLOBAL, LW_NULL);
    if (hSemaphore == LW_OBJECT_HANDLE_INVALID) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not create lwip sem.\r\n");
        SYS_STATS_INC(sem.err);
        return  (ERR_MEM);
    
    } else {
        if (psem) {
            *psem = hSemaphore;
        }
        SYS_STATS_INC(sem.used);
        
        SYS_ARCH_PROTECT(lev);
        if (lwip_stats.sys.sem.used > lwip_stats.sys.sem.max) {
            lwip_stats.sys.sem.max = lwip_stats.sys.sem.used;
        }
        SYS_ARCH_UNPROTECT(lev);
        return  (ERR_OK);
    }
}
/*********************************************************************************************************
** 函数名称: sys_sem_free
** 功能描述: 删除一个 lwip 信号量
** 输　入  : psem   信号量句柄指针
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_sem_free (sys_sem_t  *psem)
{
    API_SemaphoreCDelete(psem);
    SYS_STATS_DEC(sem.used);
}
/*********************************************************************************************************
** 函数名称: sys_sem_signal
** 功能描述: 发送一个 lwip 信号量
** 输　入  : psem   信号量句柄指针
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_sem_signal (sys_sem_t *psem)
{
    if (psem) {
        API_SemaphoreCPost(*psem);
    }
}
/*********************************************************************************************************
** 函数名称: sys_arch_sem_wait
** 功能描述: 等待一个 lwip 信号量
** 输　入  : psem   信号量句柄指针
** 输　出  : 等待时间
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t  sys_arch_sem_wait (sys_sem_t *psem, u32_t timeout)
{
    ULONG       ulError;
    ULONG       ulTimeout = (ULONG)((timeout * LW_TICK_HZ) / 1000);
                                                                        /*  转为 TICK 数                */
    ULONG       ulOldTime = API_TimeGet();
    ULONG       ulNowTime;
    
    if (psem == LW_NULL) {
        return  (SYS_ARCH_TIMEOUT);
    }
    
    if (timeout == 0) {
        ulTimeout =  LW_OPTION_WAIT_INFINITE;
    
    } else if (ulTimeout == 0) {
        ulTimeout = 1;                                                  /*  至少需要一个周期阻塞        */
    }
    
    ulError = API_SemaphoreCPend(*psem, ulTimeout);
    
    if (ulError) {
        return  (SYS_ARCH_TIMEOUT);
    
    } else {
        ulNowTime = API_TimeGet();
        ulNowTime = (ulNowTime >= ulOldTime) 
                  ? (ulNowTime -  ulOldTime) 
                  : (__ARCH_ULONG_MAX - ulOldTime + ulNowTime + 1);     /*  计算 TICK 时差              */
    
        timeout   = (u32_t)((ulNowTime * 1000) / LW_TICK_HZ);           /*  转为毫秒数                  */
        
        return  (timeout);
    }
}
/*********************************************************************************************************
** 函数名称: sys_sem_valid
** 功能描述: 获得 lwip 信号量计数器是否 > 0
** 输　入  : psem   信号量句柄指针
** 输　出  : 1: 有效 0: 无效
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
int  sys_sem_valid (sys_sem_t *psem)
{
    if (psem) {
        if (*psem) {
            return  (1);
        }
    }
    
    return  (0);
}
/*********************************************************************************************************
** 函数名称: sys_sem_set_invalid
** 功能描述: 清空 lwip 信号量计数器.
** 输　入  : psem   信号量句柄指针
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_sem_set_invalid (sys_sem_t *psem)
{
    if (psem) {
        *psem = SYS_SEM_NULL;
    }
}
/*********************************************************************************************************
** 函数名称: sys_mbox_new
** 功能描述: 创建一个 lwip 通信邮箱
** 输　入  : pmbox     需要保存的邮箱句柄
**           size      大小(忽略)
** 输　出  : 错误码
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
err_t  sys_mbox_new (sys_mbox_t *pmbox, INT  size)
{
    SYS_ARCH_DECL_PROTECT(lev);
    LW_OBJECT_HANDLE    hMsgQueueSendLock;
    LW_OBJECT_HANDLE    hMsgQueue = API_MsgQueueCreate("lwip_msg", 
                                                       LWIP_MSGQUEUE_SIZE, 
                                                       sizeof(PVOID), 
                                                       LW_OPTION_WAIT_FIFO |
                                                       LW_OPTION_OBJECT_GLOBAL,
                                                       LW_NULL);
    if (hMsgQueue == LW_OBJECT_HANDLE_INVALID) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not create lwip msgqueue.\r\n");
        SYS_STATS_INC(mbox.err);
        return  (ERR_MEM);
    
    } else {
        hMsgQueueSendLock = API_SemaphoreBCreate("lwip_msg_sendlock", LW_TRUE,
                                                 LW_OPTION_WAIT_FIFO | 
                                                 LW_OPTION_OBJECT_GLOBAL, LW_NULL);
        if (hMsgQueueSendLock == LW_OBJECT_HANDLE_INVALID) {
            API_MsgQueueDelete(&hMsgQueue);
            _DebugHandle(__ERRORMESSAGE_LEVEL, "can not create sendlock.\r\n");
            SYS_STATS_INC(mbox.err);
            return  (ERR_MEM);
        }
        __LW_MSG_QUEUE_PRIVAT(hMsgQueue) = (PVOID)hMsgQueueSendLock;
    
        if (pmbox) {
            *pmbox = hMsgQueue;
        }
        SYS_STATS_INC(mbox.used);
        
        SYS_ARCH_PROTECT(lev);
        if (lwip_stats.sys.mbox.used > lwip_stats.sys.mbox.max) {
            lwip_stats.sys.mbox.max = lwip_stats.sys.mbox.used;
        }
        SYS_ARCH_UNPROTECT(lev);
        return  (ERR_OK);
    }
}
/*********************************************************************************************************
** 函数名称: sys_mbox_free
** 功能描述: 释放一个 lwip 通信邮箱
** 输　入  : pmbox  邮箱句柄指针
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mbox_free (sys_mbox_t *pmbox)
{
    LW_OBJECT_HANDLE    hMsgQueueSendLock;
    
    if (*pmbox) {
        hMsgQueueSendLock = (LW_OBJECT_HANDLE)__LW_MSG_QUEUE_PRIVAT(*pmbox);
        API_SemaphoreBDelete(&hMsgQueueSendLock);
        API_MsgQueueDelete(pmbox);
        SYS_STATS_DEC(mbox.used);
    }
}
/*********************************************************************************************************
** 函数名称: sys_mbox_post
** 功能描述: 发送一个邮箱消息, 一定保证成功
** 输　入  : pmbox  邮箱句柄指针
**           msg    消息
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mbox_post (sys_mbox_t *pmbox, void *msg)
{
    ULONG               ulError;
    LW_OBJECT_HANDLE    hMsgQueueSendLock;
    
    if (pmbox == LW_NULL) {
        return;
    }
    
    hMsgQueueSendLock = (LW_OBJECT_HANDLE)__LW_MSG_QUEUE_PRIVAT(*pmbox);
    
    do {
        ulError = API_MsgQueueSend(*pmbox, &msg, sizeof(PVOID));
        if (ulError == ERROR_NONE) {                                    /*  发送成功                    */
            break;
        }
        
#if LW_CFG_LWIP_DEBUG > 0
        else if (ulError != ERROR_MSGQUEUE_FULL) {
            panic("sys_mbox_post() msgqueue error : %s\n", lib_strerror(errno));
            break;                                                      /*  致命错误                    */
        }
#endif                                                                  /*  LW_CFG_LWIP_DEBUG > 0       */
        
        API_SemaphoreBPend(hMsgQueueSendLock, LW_OPTION_WAIT_INFINITE); /*  等待可以发送                */
    } while (1);
}
/*********************************************************************************************************
** 函数名称: sys_mbox_trypost
** 功能描述: 发送一个邮箱消息(满则退出)
** 输　入  : pmbox  邮箱句柄指针
**           msg    消息
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
err_t  sys_mbox_trypost (sys_mbox_t *pmbox, void *msg)
{
    ULONG   ulError;
    
    if (pmbox == LW_NULL) {
        return  (ERR_MEM);
    }
    
    ulError = API_MsgQueueSend(*pmbox, &msg, sizeof(PVOID));
    if (ulError == ERROR_NONE) {                                        /*  发送成功                    */
        return  (ERR_OK);
    
    }

#if LW_CFG_LWIP_DEBUG > 0
    else if (ulError != ERROR_MSGQUEUE_FULL) {
        panic("lwip sys_mbox_trypost() msgqueue error : %s\n", lib_strerror(errno));
        return  (ERR_MEM);
    }
#endif                                                                  /*  LW_CFG_LWIP_DEBUG > 0       */
    
    else {
        return  (ERR_MEM);
    }
}
/*********************************************************************************************************
** 函数名称: sys_arch_mbox_fetch
** 功能描述: 接收一个邮箱消息
** 输　入  : pmbox  邮箱句柄指针
**           msg        消息
**           timeout    超时时间
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t   sys_arch_mbox_fetch (sys_mbox_t *pmbox, void  **msg, u32_t  timeout)
{
    ULONG       ulError;
    ULONG       ulTimeout = (ULONG)((timeout * LW_TICK_HZ) / 1000);
                                                                        /*  转为 TICK 数                */
    size_t      stMsgLen  = 0;
    PVOID       pvMsg;
    
    ULONG       ulOldTime = API_TimeGet();
    ULONG       ulNowTime;
    
    if (pmbox == LW_NULL) {
        return  (SYS_ARCH_TIMEOUT);
    }
    
    if (timeout == 0) {
        ulTimeout =  LW_OPTION_WAIT_INFINITE;
    
    } else if (ulTimeout == 0) {
        ulTimeout = 1;                                                  /*  至少需要一个周期阻塞        */
    }
    
    ulError = API_MsgQueueReceive(*pmbox, &pvMsg, sizeof(PVOID), &stMsgLen, ulTimeout);
    
    if (ulError) {
        return  (SYS_ARCH_TIMEOUT);
    
    } else {
        LW_OBJECT_HANDLE    hMsgQueueSendLock = (LW_OBJECT_HANDLE)__LW_MSG_QUEUE_PRIVAT(*pmbox);
    
        ulNowTime = API_TimeGet();
        ulNowTime = (ulNowTime >= ulOldTime) 
                  ? (ulNowTime -  ulOldTime) 
                  : (__ARCH_ULONG_MAX - ulOldTime + ulNowTime + 1);     /*  计算 TICK 时差              */
    
        timeout   = (u32_t)((ulNowTime * 1000) / LW_TICK_HZ);           /*  转为毫秒数                  */
        if (msg) {
            *msg = pvMsg;                                               /*  需要保存消息                */
        }
        
        API_SemaphoreBPost(hMsgQueueSendLock);                          /*  有空间, 允许发送数据        */
        
        return  (timeout);
    }
}
/*********************************************************************************************************
** 函数名称: sys_arch_mbox_tryfetch
** 功能描述: 无阻塞接收一个邮箱消息
** 输　入  : pmbox  邮箱句柄指针
**           msg    消息
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t   sys_arch_mbox_tryfetch (sys_mbox_t *pmbox, void  **msg)
{
    ULONG       ulError;
    size_t      stMsgLen = 0;
    PVOID       pvMsg;
    
    if (pmbox == LW_NULL) {
        return  (SYS_MBOX_EMPTY);
    }
    
    ulError = API_MsgQueueTryReceive(*pmbox, &pvMsg, sizeof(PVOID), &stMsgLen);
    if (ulError) {
        return  (SYS_MBOX_EMPTY);
    
    } else {
        LW_OBJECT_HANDLE    hMsgQueueSendLock = (LW_OBJECT_HANDLE)__LW_MSG_QUEUE_PRIVAT(*pmbox);
        
        if (msg) {
            *msg = pvMsg;                                               /*  需要保存消息                */
        }
        
        API_SemaphoreBPost(hMsgQueueSendLock);                          /*  有空间, 允许发送数据        */
        
        return  (ERR_OK);
    }
}
/*********************************************************************************************************
** 函数名称: sys_mbox_valid
** 功能描述: 获得 lwip 邮箱是否有效
** 输　入  : pmbox    邮箱句柄指针
** 输　出  : 1: 有效 0: 无效
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
int  sys_mbox_valid (sys_mbox_t *pmbox)
{
    if (pmbox) {
        if (*pmbox) {
            return  (1);
        }
    }
    
    return  (0);
}
/*********************************************************************************************************
** 函数名称: sys_mbox_set_invalid
** 功能描述: 清空 lwip 忽略邮箱中的消息
** 输　入  : pmbox    邮箱句柄指针
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_mbox_set_invalid (sys_mbox_t *pmbox)
{
    if (pmbox) {
        *pmbox = SYS_MBOX_NULL;
    }
}
/*********************************************************************************************************
  thread
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: sys_thread_hostent
** 功能描述: 多线程安全的返回一个 hostent 的拷贝
** 输　入  : phostent      hostent 信息
** 输　出  : 线程安全的拷贝
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
struct hostent  *sys_thread_hostent (struct hostent  *phostent)
{
#define __LW_MAX_HOSTENT    20

    static ip_addr_t        ipaddrBuffer[__LW_MAX_HOSTENT];
    static ip_addr_t       *pipaddrBuffer[__LW_MAX_HOSTENT];
    static struct hostent   hostentBuffer[__LW_MAX_HOSTENT];
    static int              iIndex    = 0;
    static char            *pcAliases = LW_NULL;
    
           ip_addr_t      **ppipaddrBuffer = &pipaddrBuffer[iIndex];
           struct hostent  *phostentRet    = &hostentBuffer[iIndex];


    pipaddrBuffer[iIndex] = &ipaddrBuffer[iIndex];
    ipaddrBuffer[iIndex]  = (*(ip_addr_t *)phostent->h_addr);
    
    iIndex++;
    if (iIndex >= __LW_MAX_HOSTENT) {
        iIndex =  0;
    }
    
    phostentRet->h_name      = phostent->h_name;                        /*  h_name 不需要缓冲           */
    phostentRet->h_aliases   = &pcAliases;                              /*  LW_NULL                     */
    phostentRet->h_addrtype  = phostent->h_addrtype;                    /*  AF_INET                     */
    phostentRet->h_length    = phostent->h_length;                      /*  sizeof(struct ip_addr)      */
    phostentRet->h_addr_list = (char **)ppipaddrBuffer;                 /*  ip address                  */

    return  (phostentRet);
}
/*********************************************************************************************************
** 函数名称: sys_thread_new
** 功能描述: 创建一个线程
** 输　入  : name           线程名
**           thread         线程指针
**           arg            入口参数
**           stacksize      堆栈大小
**           prio           优先级
** 输　出  : 线程句柄
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
sys_thread_t  sys_thread_new (const char *name, lwip_thread_fn thread, 
                              void *arg, int  stacksize, int prio)
{
    LW_CLASS_THREADATTR     threadattr;
    LW_OBJECT_HANDLE        hThread;
    
    if (stacksize < LW_CFG_LWIP_STK_SIZE) {
        stacksize = LW_CFG_LWIP_STK_SIZE;                               /*  最小堆栈                    */
    }
    if (prio < LW_PRIO_T_NETPROTO) {
        prio = LW_PRIO_T_NETPROTO;                                      /*  优先级不高于网络协议栈      */
    }
    if (prio > LW_PRIO_LOW) {
        prio = LW_PRIO_LOW;
    }
    
    API_ThreadAttrBuild(&threadattr,
                        stacksize,
                        (UINT8)prio,
                        (LW_OPTION_THREAD_STK_CHK | LW_OPTION_THREAD_SAFE | LW_OPTION_OBJECT_GLOBAL),
                        arg);
                                                   
    hThread = API_ThreadInit(name, (PTHREAD_START_ROUTINE)thread, &threadattr, LW_NULL);
    if (hThread == LW_OBJECT_HANDLE_INVALID) {
        _DebugHandle(__ERRORMESSAGE_LEVEL, "can not create lwip thread.\r\n");
        return  (hThread);
    }
    
    /*
     *  这里可以加入一些特殊的处理...
     */
    API_ThreadStart(hThread);

    return  (hThread);
}
/*********************************************************************************************************
  time
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: sys_jiffies
** 功能描述: 返回系统时钟
** 输　入  : NONE
** 输　出  : 系统时钟
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t  sys_jiffies (void)
{
    return  ((u32_t)API_TimeGet());
}
/*********************************************************************************************************
** 函数名称: sys_arch_msleep
** 功能描述: 延迟 ms 
** 输　入  : ms  需要延迟的 ms
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sys_arch_msleep (u32_t ms)
{
    API_TimeMSleep((ULONG)ms);
}
/*********************************************************************************************************
** 函数名称: sys_now
** 功能描述: 返回当前时间 (单位 : ms) 
** 输　入  : NONE
** 输　出  : 当前时间
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t  sys_now (void)
{
    UINT64      ullTemp = (UINT64)API_TimeGet64();                      /*  使用 64 位, 避免溢出        */
    
    ullTemp = ((ullTemp * 1000) / LW_TICK_HZ);                          /*  变换为毫秒计数              */
    
    return  ((u32_t)(ullTemp % ((u32_t)~0)));                           /*  返回                        */
}
/*********************************************************************************************************
  PPP/SLIP
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: sio_open
** 功能描述: sylixos sio open
** 输　入  : port      COM 端口
** 输　出  : 文件描述符
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
sio_fd_t  sio_open (u8_t  port)
{
    CHAR    cNameBuffer[64];
    int     iFd;
    
    snprintf(cNameBuffer, sizeof(cNameBuffer), 
             "%s%d", LWIP_SYLIXOS_TTY_NAME, port);                      /*  合成文件名                  */
    
    __KERNEL_SPACE_ENTER();                                             /*  内核文件描述符              */
    iFd = open(cNameBuffer, O_RDWR);
    if (iFd < 0) {
        __KERNEL_SPACE_EXIT();
        fprintf(stderr, "sio_open() can not open file : \"%s\".\n", cNameBuffer);
        return  (0);                                                    /*  无法打开文件                */
    }
    ioctl(iFd, FIOOPTIONS, OPT_RAW);                                    /*  进入原始模式                */
    ioctl(iFd, FIORTIMEOUT, LW_NULL);                                   /*  无限长等待时间              */
    __KERNEL_SPACE_EXIT();
    
    return  ((sio_fd_t)iFd);
}
/*********************************************************************************************************
** 函数名称: sio_send
** 功能描述: sylixos sio send
** 输　入  : data      数据
**           fd        文件描述符
** 输　出  : 
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sio_send (u8_t  data, sio_fd_t  fd)
{
    __KERNEL_SPACE_ENTER();                                             /*  内核文件描述符              */
    write((int)fd, (const void *)&data, 1);
    __KERNEL_SPACE_EXIT();
}
/*********************************************************************************************************
** 函数名称: sio_recv
** 功能描述: sylixos sio recv
** 输　入  : fd        文件描述符
** 输　出  : 数据
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u8_t  sio_recv (sio_fd_t  fd)
{
    char    data;
    
    __KERNEL_SPACE_ENTER();                                             /*  内核文件描述符              */
    read((int)fd, (void *)&data, 1);
    __KERNEL_SPACE_EXIT();
    
    return  ((u8_t)data);
}
/*********************************************************************************************************
** 函数名称: sio_read
** 功能描述: sylixos sio read
** 输　入  : fd        文件描述符
**           buffer    缓冲区
**           num       个数
** 输　出  : 个数
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t  sio_read (sio_fd_t  fd, u8_t *buffer, u32_t  num)
{
    ssize_t     sstReadNum;
    
    __KERNEL_SPACE_ENTER();
    sstReadNum = read((int)fd, (void *)buffer, (size_t)num);
    __KERNEL_SPACE_EXIT();

    if (sstReadNum < 0) {
        return  (0);
    
    } else {
        return  ((u32_t)sstReadNum);
    }
}
/*********************************************************************************************************
** 函数名称: sio_tryread
** 功能描述: sylixos sio try read (if no data is available and never blocks)
** 输　入  : fd        文件描述符
**           buffer    缓冲区
**           num       个数
** 输　出  : 个数
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t  sio_tryread (sio_fd_t  fd, u8_t *buffer, u32_t  num)
{
    INT    iNRead = 0;
    u32_t  uiRead = 0;

    __KERNEL_SPACE_ENTER();
    if (ioctl((int)fd, FIONREAD, &iNRead)) {
        __KERNEL_SPACE_EXIT();
        return  (0);
    }
    
    if (iNRead > 0) {
        uiRead = (u32_t)read((int)fd, (void *)buffer, num);
    }
    __KERNEL_SPACE_EXIT();
    
    return  (uiRead);
}
/*********************************************************************************************************
** 函数名称: sio_write
** 功能描述: sylixos sio write
** 输　入  : fd        文件描述符
**           buffer    缓冲区
**           num       个数
** 输　出  : 个数
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
u32_t  sio_write (sio_fd_t  fd, u8_t *buffer, u32_t  num)
{
    ssize_t     ssWriteNum;
    
    __KERNEL_SPACE_ENTER();
    ssWriteNum = write((int)fd, (const void *)buffer, (size_t)num);
    __KERNEL_SPACE_EXIT();
    
    if (ssWriteNum < 0) {
        return  (0);
    
    } else {
        return  ((u32_t)ssWriteNum);
    }
}
/*********************************************************************************************************
** 函数名称: sio_read_abort
** 功能描述: sylixos sio read abort
** 输　入  : fd        文件描述符
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
void  sio_read_abort (sio_fd_t  fd)
{
    __KERNEL_SPACE_ENTER();
    ioctl((int)fd, FIOWAITABORT, OPT_RABORT);                           /*  解除一个读阻塞              */
    __KERNEL_SPACE_EXIT();
}
/*********************************************************************************************************
  ip extern 
*********************************************************************************************************/
/*********************************************************************************************************
** 函数名称: ip_input_hook
** 功能描述: sylixos ip input hook
** 输　入  : pvPBuf        pbuf
**           pvNetif       net interface
** 输　出  : 是否被 eaten
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
int ip_input_hook (PVOID  pvPBuf, PVOID  pvNetif)
{
#if LW_CFG_NET_NAT_EN > 0
extern VOID  nat_ip_input_hook(struct pbuf *p, struct netif *inp);
#endif                                                                  /*  LW_CFG_NET_NAT_EN > 0       */

    struct pbuf  *p   = (struct pbuf  *)pvPBuf;
    struct netif *inp = (struct netif *)pvNetif;
    
    (VOID)p;
    (VOID)inp;
    
#if LW_CFG_NET_NAT_EN > 0
    nat_ip_input_hook(p, inp);
#endif                                                                  /*  LW_CFG_NET_NAT_EN > 0       */

    return  (0);                                                        /*  do not eaten packet         */
}
/*********************************************************************************************************
** 函数名称: ip_route_hook
** 功能描述: sylixos ip route hook
** 输　入  : dest  destination route netif
** 输　出  : netif
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
PVOID ip_route_hook (const PVOID pvDest)
{
extern struct netif  *sys_ip_route_hook(const ip_addr_t *ipaddrDest);

    const  ip_addr_t *dest = (const ip_addr_t *)pvDest;
    struct netif     *netif;
    
    netif = sys_ip_route_hook(dest);

    return ((PVOID)netif);
}
/*********************************************************************************************************
** 函数名称: link_input_hook
** 功能描述: sylixos link input hook (没有在 CORELOCK 锁中)
** 输　入  : pvPBuf        pbuf
**           pvNetif       net interface
** 输　出  : 是否被 eaten
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
int link_input_hook (PVOID  pvPBuf, PVOID  pvNetif)
{
extern INT packet_link_input(struct pbuf *p, struct netif *inp, BOOL bOutgo);

    if (!netif_is_up((struct netif *)pvNetif)) {
        return  (1);                                                    /*  没有使能的网卡不接收        */
    }

    return  (packet_link_input((struct pbuf *)pvPBuf, (struct netif *)pvNetif, LW_FALSE));
}
/*********************************************************************************************************
** 函数名称: link_output_hook
** 功能描述: sylixos link output hook (在 CORELOCK 锁中)
** 输　入  : pvPBuf        pbuf
**           pvNetif       net interface
** 输　出  : 是否被 eaten
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
int link_output_hook (PVOID  pvPBuf, PVOID  pvNetif)
{
extern INT packet_link_input(struct pbuf *p, struct netif *inp, BOOL bOutgo);

    return  (packet_link_input((struct pbuf *)pvPBuf, (struct netif *)pvNetif, LW_TRUE));
}
/*********************************************************************************************************
** 函数名称: ppp_link_status_hook
** 功能描述: sylixos provide ppp status call back
** 输　入  : pcb       pcb
**           iError    error code
**           pvArg     arg
** 输　出  : netif
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if PPP_SUPPORT > 0 || PPPOE_SUPPORT > 0

VOID ppp_link_status_hook (PVOID pvPPP, INT iError, PVOID pvArg)
{
    ppp_pcb *pcb = (ppp_pcb *)pvPPP;
    
    char    *pcError = "<unknown>";
    char     cNetifName[16];
    char    *pcNetifName;
    
    switch (iError) {
    
    case PPPERR_NONE:
        pcError = "link is up";
        break;
        
    case PPPERR_PARAM:
        pcError = "invalid parameter";
        break;
        
    case PPPERR_OPEN:
        pcError = "unable to open PPP session";
        break;
        
    case PPPERR_DEVICE:
        pcError = "invalid I/O device for PPP";
        break;
        
    case PPPERR_ALLOC:
        pcError = "unable to allocate resources";
        break;
        
    case PPPERR_USER:
        pcError = "user interrupt";
        break;
        
    case PPPERR_CONNECT:
        pcError = "connection lost";
        break;
        
    case PPPERR_AUTHFAIL:
        pcError = "failed authentication challenge";
        break;
        
    case PPPERR_PROTOCOL:
        pcError = "failed to meet protocol";
        break;
        
    case PPPERR_PEERDEAD:
        pcError = "connection timeout";
        break;
        
    case PPPERR_IDLETIMEOUT:
        pcError = "idle Timeout";
        break;
        
    case PPPERR_CONNECTTIME:
        pcError = "max connect time reached";
        break;
        
    case PPPERR_LOOPBACK:
        pcError = "loopback detected";
        break;
    }
    
    pcNetifName = if_indextoname(pcb->netif->num, cNetifName);
    if (!pcNetifName) {
        pcNetifName = "<unknown>";
    }
    
    if (iError == PPPERR_NONE) {
        printk(KERN_INFO "ppp %s status : %s.\n", pcNetifName, pcError);
    
    } else {
        printk(KERN_ERR "ppp %s status : %s.\n", pcNetifName, pcError);
    }
}

#endif                                                                  /*  PPP_SUPPORT > 0 ||          */
                                                                        /*  PPPOE_SUPPORT > 0           */
/*********************************************************************************************************
** 函数名称: htonl
** 功能描述: inet htonl
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
uint32_t htonl (uint32_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    return  LWIP_PLATFORM_HTONL(x);
#else
    return  x;
#endif
}
/*********************************************************************************************************
** 函数名称: ntohl
** 功能描述: inet ntohl
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
uint32_t ntohl (uint32_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    return  LWIP_PLATFORM_HTONL(x);
#else
    return  x;
#endif
}
/*********************************************************************************************************
** 函数名称: htons
** 功能描述: inet htons
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
uint16_t htons (uint16_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    return  LWIP_PLATFORM_HTONS(x);
#else
    return  x;
#endif
}
/*********************************************************************************************************
** 函数名称: ntohs
** 功能描述: inet ntohs
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
uint16_t ntohs (uint16_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
    return  LWIP_PLATFORM_HTONS(x);
#else
    return  x;
#endif
}
#endif                                                                  /*  LW_CFG_NET_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
