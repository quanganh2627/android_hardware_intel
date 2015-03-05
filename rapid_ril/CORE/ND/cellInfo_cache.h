////////////////////////////////////////////////////////////////////////////
// cellInfo_cache.h
//
// Copyright 2009 Intrinsyc Software International, Inc.  All rights reserved.
// Patents pending in the United States of America and other jurisdictions.
//
//
// Description:
//    Defines the cellInfo cache class which caches the cell information of
//    neighbouring and serving cells.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef RRIL_CELLINFO_CACHE_H
#define RRIL_CELLINFO_CACHE_H

#include "types.h"
#include "nd_structs.h"
#include "sync_ops.h"


class CellInfoCache
{
public:
    CellInfoCache();
    ~CellInfoCache();
    BOOL updateCache(const P_ND_N_CELL_INFO_DATA pData, const INT32 aItemsCount);
    BOOL getCellInfo(P_ND_N_CELL_INFO_DATA pRetData, UINT32& uiItemCount);
    bool IsCellInfoCacheEmpty() { return m_iCacheSize <= 0; }

private:
    INT32 checkCache(const RIL_CellInfo& pData);
    S_ND_N_CELL_INFO_DATA m_sCellInfo;
    INT32 m_iCacheSize;
    CMutex* m_pCacheLock;
};

#endif
