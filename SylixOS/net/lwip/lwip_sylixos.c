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
** 文   件   名: lwip_sylixos.c
**
** 创   建   人: Han.Hui (韩辉)
**
** 文件创建日期: 2009 年 05 月 06 日
**
** 描        述: lwip sylixos 接口.

** BUG:
2009.05.20  _netJobqueueInit 初始化应放在网络初始化的后面, 更加安全.
2009.07.11  API_NetInit() 仅允许初始化一次.
2009.08.19  去掉 snmp init 这个操作将在 lwip 1.3.1 以后版本中自动初始化.
2010.02.22  升级 lwip 后去掉 loopif 的初始化(协议栈自行安装回环网口).
2010.07.28  加入 snmp 时间戳回调和独立的初始化函数.
2010.11.01  __netCloseAll() 调用 DHCP 函数使用安全模式.
2012.12.18  加入 unix_init() 初始化 AF_UNIX 域协议.
2013.06.21  加入网络 proc 文件.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "../SylixOS/kernel/include/k_kernel.h"
#include "../SylixOS/system/include/s_system.h"
/*********************************************************************************************************
  裁剪控制
*********************************************************************************************************/
#if LW_CFG_NET_EN > 0
#include "lwip/tcpip.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/snmp.h"
#include "lwip/snmp_msg.h"
#include "lwip/sockets.h"
#include "./unix/af_unix.h"
#include "./packet/af_packet.h"
#if LW_CFG_PROCFS_EN > 0
#include "./proc/lwip_proc.h"
#endif                                                                  /*  LW_CFG_PROCFS_EN            */
/*********************************************************************************************************
  网络驱动工作队列函数声明
*********************************************************************************************************/
INT   _netJobqueueInit(VOID);
/*********************************************************************************************************
  网络函数声明
*********************************************************************************************************/
#if LW_CFG_SHELL_EN > 0
VOID  __tshellNetInit(VOID);
VOID  __tshellNet6Init(VOID);
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
/*********************************************************************************************************
  互联网函数声明
*********************************************************************************************************/
VOID  __inetHostTableInit(VOID);
/*********************************************************************************************************
  网络事件初始化
*********************************************************************************************************/
INT   _netEventInit(VOID);
INT   _netEventDevCreate(VOID);
/*********************************************************************************************************
  拨号网络函数声明
*********************************************************************************************************/
#if LW_CFG_LWIP_PPP > 0 || LW_CFG_LWIP_PPPOE > 0
#if __LWIP_USE_PPP_NEW == 0
err_t pppInit(void);
#endif
#endif                                                                  /*  LW_CFG_LWIP_PPP             */
                                                                        /*  LW_CFG_LWIP_PPPOE           */
/*********************************************************************************************************
** 函数名称: __netCloseAll
** 功能描述: 系统重启或关闭时回调函数.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
static VOID  __netCloseAll (VOID)
{
    PLW_DEV_HDR      pdevhdr;
    PCHAR            pcTail;
    
    struct netif    *netif = netif_list;
    
    pdevhdr = iosDevFind(LWIP_SYLIXOS_SOCKET_NAME, &pcTail);
    if (pdevhdr) {
        iosDevFileAbnormal(pdevhdr);                                    /*  关闭所有网络文件            */
    }                                                                   /*  同时将相关文件设置为异常模式*/
    
#if LWIP_DHCP > 0
    for (; netif != LW_NULL; netif = netif->next) {
        if ((netif->flags & NETIF_FLAG_DHCP) && (netif->dhcp)) {
            netifapi_netif_common(netif, NULL, dhcp_release);           /*  解除 DHCP 租约, 同时停止网卡*/
            netifapi_dhcp_stop(netif);                                  /*  释放资源                    */
        }
    }
#else
    (VOID)netif;
#endif                                                                  /*  LWIP_DHCP > 0               */
}
/*********************************************************************************************************
** 函数名称: __netSnmpTimestamp
** 功能描述: 获取 SNMP 时间戳.
** 输　入  : puiTimestamp       SNMP 时间戳 (以 10ms 为单位!)
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
VOID  __netSnmpGetTimestamp (UINT32  *puiTimestamp)
{
    INT64       i64Ticks = API_TimeGet64();                             /*  获取系统时钟                */
    
    if (puiTimestamp) {
        /*
         *  uiTimestamp 是以 10 毫秒为单位
         */
        *puiTimestamp = (UINT32)((i64Ticks * LW_CFG_TICKS_PER_SEC * 10) / 1000);
    }
}
/*********************************************************************************************************
** 函数名称: __netSnmpInit
** 功能描述: 初始化 SNMP 默认信息.
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
*********************************************************************************************************/
#if LWIP_SNMP > 0

static VOID  __netSnmpInit (VOID)
{
    static u8_t     ucContactLen  = 17;
    static u8_t     ucLocationLen = 6;
    
    static u8_t     ucDesrLen = 7;
    static u8_t     ucNameLen = 23;
    
    snmp_set_syscontact((u8_t *)"sylixos@gmail.com", &ucContactLen);
    snmp_set_syslocation((u8_t *)"@china", &ucLocationLen);             /*  at CHINA ^_^                */
    
    snmp_set_sysdescr((u8_t *)"sylixos", &ucDesrLen);
    snmp_set_sysname((u8_t *)"device based on sylixos", &ucNameLen);
}

#endif                                                                  /*  LWIP_SNMP > 0               */
/*********************************************************************************************************
** 函数名称: API_NetInit
** 功能描述: 向操作系统内核注册网络组件
** 输　入  : NONE
** 输　出  : NONE
** 全局变量: 
** 调用模块: 
                                           API 函数
*********************************************************************************************************/
LW_API  
VOID  API_NetInit (VOID)
{
    static BOOL            bIsInit = LW_FALSE;
    
    if (bIsInit) {
        return;
    } else {
        bIsInit = LW_TRUE;
    }
    
    _netJobqueueInit();                                                 /*  创建网络驱动作业处理队列    */
    _netEventInit();                                                    /*  初始化网络事件              */
    _netEventDevCreate();                                               /*  创建网络事件设备            */
    
    API_SystemHookAdd((LW_HOOK_FUNC)__netCloseAll, 
                      LW_OPTION_KERNEL_REBOOT);                         /*  安装系统关闭时, 回调函数    */
                      
    tcpip_init(LW_NULL, LW_NULL);                                       /*  以多任务形式初始化 lwip     */
    
    unix_init();                                                        /*  初始化 AF_UNIX 域协议       */
    
    packet_init();                                                      /*  初始化 AF_PACKET 协议       */
    
    __socketInit();                                                     /*  初始化 socket 系统          */

#if LW_CFG_LWIP_PPP > 0 || LW_CFG_LWIP_PPPOE > 0
#if __LWIP_USE_PPP_NEW == 0
    pppInit();                                                          /*  初始化 point to point proto */
#endif                                                                  /*  !__LWIP_USE_PPP_NEW         */
#endif                                                                  /*  LW_CFG_LWIP_PPP             */
                                                                        /*  LW_CFG_LWIP_PPPOE           */
    
#if LWIP_SNMP > 0
    __netSnmpInit();                                                    /*  初始化 SNMP 基本信息        */
#endif                                                                  /*  LWIP_SNMP > 0               */
    
#if LW_CFG_SHELL_EN > 0
    __tshellNetInit();                                                  /*  注册网络命令                */
    __tshellNet6Init();                                                 /*  注册 IPv6 专属命令          */
#endif                                                                  /*  LW_CFG_SHELL_EN > 0         */
    
    /*
     *  密切相关工具初始化.
     */
    __inetHostTableInit();                                              /*  初始化本地地址转换表        */
    
#if LW_CFG_PROCFS_EN > 0
    __procFsNetInit();
#endif                                                                  /*  LW_CFG_PROCFS_EN > 0        */
}
#endif                                                                  /*  LW_CFG_NET_EN               */
/*********************************************************************************************************
  END
*********************************************************************************************************/
