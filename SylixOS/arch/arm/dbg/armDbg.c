/*********************************************************************************************************
**
**                                    �й�������Դ��֯
**
**                                   Ƕ��ʽʵʱ����ϵͳ
**
**                                SylixOS(TM)  LW : long wing
**
**                               Copyright All Rights Reserved
**
**--------------�ļ���Ϣ--------------------------------------------------------------------------------
**
** ��   ��   ��: armDbg.c
**
** ��   ��   ��: Han.Hui (����)
**
** �ļ���������: 2014 �� 05 �� 14 ��
**
** ��        ��: ARM ��ϵ���ܵ������.
*********************************************************************************************************/
#define  __SYLIXOS_KERNEL
#include "SylixOS.h"
/*********************************************************************************************************
  �ü�����
*********************************************************************************************************/
#if LW_CFG_GDB_EN > 0
#include "dtrace.h"
#if LW_CFG_CACHE_EN > 0
#include "../mm/cache/armCacheCommon.h"
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
/*********************************************************************************************************
  ARM �ϵ�ʹ��δ����ָ���쳣
*********************************************************************************************************/
#define ARM_BREAKPOINT_INS          0xE7FFDEFE
#define ARM_ABORTPOINT_INS          0xE7FFDDFE
/*********************************************************************************************************
** ��������: archDbgBpInsert
** ��������: ����һ���ϵ�.
** �䡡��  : ulAddr         �ϵ��ַ
**           pulIns         ���ص�֮ǰ��ָ��
** �䡡��  : NONE
** ȫ�ֱ���: 
** ����ģ��: 
*********************************************************************************************************/
VOID  archDbgBpInsert (addr_t  ulAddr, ULONG  *pulIns)
{
    *pulIns = *(ULONG *)ulAddr;
    *(ULONG *)ulAddr = ARM_BREAKPOINT_INS;
    
#if LW_CFG_CACHE_EN > 0
    armDCacheFlushMVA((PVOID)ulAddr);
    armICacheInvalidate((PVOID)ulAddr, sizeof(ULONG));
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
}
/*********************************************************************************************************
** ��������: archDbgAbInsert
** ��������: ����һ���쳣��.
** �䡡��  : ulAddr         �ϵ��ַ
**           pulIns         ���ص�֮ǰ��ָ��
** �䡡��  : NONE
** ȫ�ֱ���: 
** ����ģ��: 
*********************************************************************************************************/
VOID  archDbgAbInsert (addr_t  ulAddr, ULONG  *pulIns)
{
    *pulIns = *(ULONG *)ulAddr;
    *(ULONG *)ulAddr = ARM_ABORTPOINT_INS;
    
#if LW_CFG_CACHE_EN > 0
    armDCacheFlushMVA((PVOID)ulAddr);
    armICacheInvalidate((PVOID)ulAddr, sizeof(ULONG));
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
}
/*********************************************************************************************************
** ��������: archDbgBpRemove
** ��������: ɾ��һ���ϵ�.
** �䡡��  : ulAddr         �ϵ��ַ
**           pulIns         ���ص�֮ǰ��ָ��
** �䡡��  : NONE
** ȫ�ֱ���: 
** ����ģ��: 
*********************************************************************************************************/
VOID  archDbgBpRemove (addr_t  ulAddr, ULONG  ulIns)
{
    *(ULONG *)ulAddr = ulIns;
    
#if LW_CFG_CACHE_EN > 0
    armDCacheFlushMVA((PVOID)ulAddr);
    armICacheInvalidate((PVOID)ulAddr, sizeof(ULONG));
#endif                                                                  /*  LW_CFG_CACHE_EN > 0         */
}
/*********************************************************************************************************
** ��������: archDbgTrapType
** ��������: ��ȡ trap ����.
** �䡡��  : ulAddr         �ϵ��ַ
** �䡡��  : LW_TRAP_INVAL / LW_TRAP_BRKPT / LW_TRAP_ABORT
** ȫ�ֱ���: 
** ����ģ��: 
*********************************************************************************************************/
UINT  archDbgTrapType (addr_t  ulAddr)
{
    switch (*(ULONG *)ulAddr) {
    
    case ARM_BREAKPOINT_INS:
        return  (LW_TRAP_BRKPT);
        
    case ARM_ABORTPOINT_INS:
        return  (LW_TRAP_ABORT);
        
    default:
        return  (LW_TRAP_INVAL);
    }
}
#endif                                                                  /*  LW_CFG_GDB_EN > 0           */
/*********************************************************************************************************
  END
*********************************************************************************************************/