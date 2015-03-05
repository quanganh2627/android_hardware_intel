////////////////////////////////////////////////////////////////////////////
// cellInfo_cache.cpp
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Implements the cellInfo cache class which caches the cell information of
//    neighbouring and serving cells.
//
/////////////////////////////////////////////////////////////////////////////

#include "cellInfo_cache.h"
#include "util.h"
#include "rillog.h"
#include "rril.h"

BOOL operator==(const RIL_CellIdentityGsm& lhs, const RIL_CellIdentityGsm& rhs)
{
    return (lhs.mnc == rhs.mnc && lhs.mcc == rhs.mcc && lhs.lac == rhs.lac && lhs.cid == rhs.cid);
}

BOOL operator==(const RIL_CellIdentityWcdma& lhs, const RIL_CellIdentityWcdma& rhs)
{
    return (lhs.mnc == rhs.mnc && lhs.mcc == rhs.mcc && lhs.lac == rhs.lac
            && lhs.cid == rhs.cid && lhs.psc == rhs.psc);
}

BOOL operator==(const RIL_CellIdentityLte& lhs, const RIL_CellIdentityLte& rhs)
{
    return (lhs.mnc == rhs.mnc && lhs.mcc == rhs.mcc && lhs.ci == rhs.ci
            && lhs.pci == rhs.pci && lhs.tac == rhs.tac);
}

BOOL operator==(const RIL_GW_SignalStrength& lhs, const RIL_GW_SignalStrength& rhs)
{
    return (lhs.signalStrength == rhs.signalStrength && lhs.bitErrorRate == rhs.bitErrorRate);
}

BOOL operator==(const RIL_SignalStrengthWcdma& lhs, const RIL_SignalStrengthWcdma& rhs)
{
    return (lhs.signalStrength == rhs.signalStrength && lhs.bitErrorRate == rhs.bitErrorRate);
}

BOOL operator==(const RIL_LTE_SignalStrength_v8& lhs, const RIL_LTE_SignalStrength_v8& rhs)
{
    return (lhs.signalStrength == rhs.signalStrength && lhs.rsrp == rhs.rsrp
            && lhs.rsrq == rhs.rsrq && lhs.rssnr == rhs.rssnr
            && lhs.cqi == rhs.cqi && lhs.timingAdvance == rhs.timingAdvance);
}

BOOL operator==(const RIL_CellInfo& first, const RIL_CellInfo& second)
{
     // Check whether cell info type is the same.
     // if yes, based on the type, check the values
     if (first.cellInfoType == second.cellInfoType)
     {
         switch(first.cellInfoType)
         {
         case RIL_CELL_INFO_TYPE_GSM:
         {
             RIL_CellIdentityGsm gsm1 = first.CellInfo.gsm.cellIdentityGsm;
             RIL_CellIdentityGsm gsm2 = second.CellInfo.gsm.cellIdentityGsm;
             RIL_GW_SignalStrength sgGsm1 = first.CellInfo.gsm.signalStrengthGsm;
             RIL_GW_SignalStrength sgGsm2 = second.CellInfo.gsm.signalStrengthGsm;
             return (gsm1 == gsm2 && sgGsm1 == sgGsm2);
         }
         case RIL_CELL_INFO_TYPE_WCDMA:
         {
             RIL_CellIdentityWcdma wcdma1 = first.CellInfo.wcdma.cellIdentityWcdma;
             RIL_CellIdentityWcdma wcdma2 = second.CellInfo.wcdma.cellIdentityWcdma;
             RIL_SignalStrengthWcdma sgWcdma1 = first.CellInfo.wcdma.signalStrengthWcdma;
             RIL_SignalStrengthWcdma sgWcdma2 = second.CellInfo.wcdma.signalStrengthWcdma;
             return (wcdma1 == wcdma2 && sgWcdma1 == sgWcdma2);
         }
         case RIL_CELL_INFO_TYPE_LTE:
         {
             RIL_CellIdentityLte lte1 = first.CellInfo.lte.cellIdentityLte;
             RIL_CellIdentityLte lte2 = second.CellInfo.lte.cellIdentityLte;
             RIL_LTE_SignalStrength_v8 sgLte1 = first.CellInfo.lte.signalStrengthLte;
             RIL_LTE_SignalStrength_v8 sgLte2 = second.CellInfo.lte.signalStrengthLte;
             return (lte1 == lte2 && sgLte1 == sgLte2);
         }
         default:
             return FALSE;
         }
     }
     return FALSE;
}

CellInfoCache::CellInfoCache()
{
    memset(&m_sCellInfo, 0, sizeof(S_ND_N_CELL_INFO_DATA));
    m_iCacheSize = 0;
    m_pCacheLock = new CMutex();
}

CellInfoCache::~CellInfoCache()
{
    delete m_pCacheLock;
}

BOOL CellInfoCache::getCellInfo(P_ND_N_CELL_INFO_DATA pRetData, UINT32& uiItemCount)
{
    if (pRetData == NULL)
        return FALSE;
    // loop through the cache and copy the info to the pRetData
    CMutex::Lock(m_pCacheLock);
    uiItemCount = m_iCacheSize;
    for (INT32 i = 0; i < m_iCacheSize; i++)
    {
        pRetData->pnCellData[i] = m_sCellInfo.pnCellData[i];
    }
    CMutex::Unlock(m_pCacheLock);
    return TRUE;
}

INT32 CellInfoCache::checkCache(const RIL_CellInfo& pData)
{
    RIL_LOG_VERBOSE("CellInfoCache::checkCache() %d\r\n", m_iCacheSize);
    for (INT32 i = 0; i < m_iCacheSize; i++)
    {
        if (pData == m_sCellInfo.pnCellData[i])
        {
            RIL_LOG_VERBOSE("CellInfoCache::checkCache() - Found match at %d\r\n",i);
            return i;
        }
    }
    return -1;
}

BOOL CellInfoCache::updateCache(const P_ND_N_CELL_INFO_DATA pData, const INT32 aItemsCount)
{
    BOOL ret = FALSE;

    RIL_LOG_VERBOSE("CellInfoCache::updateCache() - aItemsCount %d \r\n",aItemsCount);
    if (NULL == pData || aItemsCount > RRIL_MAX_CELL_ID_COUNT)
    {
        RIL_LOG_INFO("CellInfoCache::updateCache() - Invalid data\r\n");
        goto Error;
    }
    else
    {
        // if there were more items in the cache before
        if (aItemsCount != m_iCacheSize)
        {
            ret = TRUE;
        }
        else
        {
            for (INT32 storeIndex= 0; storeIndex < aItemsCount; storeIndex++)
            {
                if (checkCache(pData->pnCellData[storeIndex]) < 0 ) // new item
                {
                    ret = TRUE;
                    break;
                }
            }
        }
    }

    if (ret)
    {
        RIL_LOG_INFO("CellInfoCache::updateCache() -"
                "Updating cache with %d items\r\n", aItemsCount);
        // Access mutex
        CMutex::Lock(m_pCacheLock);
        m_iCacheSize = aItemsCount;
        memset(&m_sCellInfo, 0, sizeof(S_ND_N_CELL_INFO_DATA));
        for (INT32 j = 0; j < m_iCacheSize; j++)
        {
            m_sCellInfo.pnCellData[j] = pData->pnCellData[j];
        }
        // release mutex
        CMutex::Unlock(m_pCacheLock);
    }
Error:
    return ret;
}
