/**
 * @file
 *
 * AMD CPU POST API, and related functions.
 *
 * Contains code that initialized the CPU after memory init.
 *
 * @xrefitem bom "File Content Label" "Release Content"
 * @e project:      AGESA
 * @e sub-project:  CPU
 * @e \$Revision: 6708 $   @e \$Date: 2008-07-10 18:04:19 -0500 (Thu, 10 Jul 2008) $
 *
 */
/*
 ****************************************************************************
 * AMD Generic Encapsulated Software Architecture
 *
 * Description: cpuPostInit.c - Cpu POST Initialization Functions.
 *
 ******************************************************************************
 *
 * Copyright (c) 2011, Advanced Micro Devices, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the names of 
 *       its contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ADVANCED MICRO DEVICES, INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 ******************************************************************************
 */

/*----------------------------------------------------------------------------------------
 *                             M O D U L E S    U S E D
 *----------------------------------------------------------------------------------------
 */

#include "AGESA.h"
#include "amdlib.h"
#include "Ids.h"
#include "Options.h"
#include "cpuRegisters.h"
#include "cpuApicUtilities.h"
#include "heapManager.h"
#include "cpuServices.h"
#include "cpuFeatures.h"
#include "GeneralServices.h"
#include "cpuPostInit.h"
#include "cpuPstateTables.h"
#include "cpuFamilyTranslation.h"
#include "Filecode.h"
#define FILECODE PROC_CPU_CPUPOSTINIT_FILECODE
/*----------------------------------------------------------------------------------------
 *                   D E F I N I T I O N S    A N D    M A C R O S
 *----------------------------------------------------------------------------------------
 */

/*----------------------------------------------------------------------------------------
 *                  T Y P E D E F S     A N D     S T R U C T U R E S
 *----------------------------------------------------------------------------------------
 */

/*----------------------------------------------------------------------------------------
 *           P R O T O T Y P E S     O F     L O C A L     F U N C T I O N S
 *----------------------------------------------------------------------------------------
 */
VOID
STATIC
SyncVariableMTRR (
  IN       AMD_CONFIG_PARAMS  *StdHeader
  );

VOID
STATIC
SetCoresTscFreqSel (
  IN       AMD_CONFIG_PARAMS   *StdHeader
  );
/*----------------------------------------------------------------------------------------
 *                          E X P O R T E D    F U N C T I O N S
 *----------------------------------------------------------------------------------------
 */
extern BUILD_OPT_CFG UserOptions;
extern CPU_FAMILY_SUPPORT_TABLE            PstateFamilyServiceTable;

extern
VOID
ExecuteWbinvdInstruction (
  IN OUT   AMD_CONFIG_PARAMS *StdHeader
  );

AGESA_STATUS
PstateCreateHeapInfo (
  IN       AMD_CONFIG_PARAMS *StdHeader
  );

/*---------------------------------------------------------------------------------------*/
/**
 * Performs CPU related initialization at the POST entry point
 *
 * This function performs a large list of initialization items.  These items
 * include:
 *
 *    -1      AP MTRR sync
 *    -2      feature leveling
 *    -3      P-state data gather
 *    -4      P-state leveling
 *    -5      AP cache breakdown & release
 *
 * @param[in]  StdHeader         Config handle for library and services
 * @param[in]  PlatformConfig    Config handle for platform specific information
 *
 * @retval     AGESA_SUCCESS
 *
 */
AGESA_STATUS
AmdCpuPost (
  IN       AMD_CONFIG_PARAMS *StdHeader,
  IN       PLATFORM_CONFIGURATION *PlatformConfig
  )
{
  AGESA_STATUS AgesaStatus;
  AGESA_STATUS CalledStatus;

  AgesaStatus = AGESA_SUCCESS;
  //
  // Sync variable MTRR
  //
  AGESA_TESTPOINT (TpProcCpuApMtrrSync, StdHeader);
  SyncVariableMTRR (StdHeader);

  AGESA_TESTPOINT (TpProcCpuPostFeatureInit, StdHeader);
  CalledStatus = DispatchCpuFeatures (CPU_FEAT_AFTER_POST_MTRR_SYNC, PlatformConfig, StdHeader);
  if (CalledStatus > AgesaStatus) {
    AgesaStatus = CalledStatus;
  }
  //
  // Feature Leveling
  //
  AGESA_TESTPOINT (TpProcCpuFeatureLeveling, StdHeader);
  FeatureLeveling (StdHeader);
  //
  // P-state Gathered and set heap info
  //
  PstateCreateHeapInfo (StdHeader);

  // Set TscFreqSel at the rate specified by the core P0 after core frequency leveling.
  SetCoresTscFreqSel (StdHeader);

  // Relinquish control of all APs to IBV.
  RelinquishControlOfAllAPs (StdHeader);

  return (AgesaStatus);
}

/*---------------------------------------------------------------------------------------
 *                           L O C A L    F U N C T I O N S
 *---------------------------------------------------------------------------------------
 */

/*---------------------------------------------------------------------------------------*/
/**
 * Determines the address in system DRAM that should be used for p-state data
 * gather and leveling.
 *
 * @param[out] Ptr               Address to utilize
 * @param[in]  StdHeader         Config handle for library and services
 *
 * @retval     AGESA_SUCCESS
 *
 */
STATIC AGESA_STATUS
GetPstateGatherDataAddressAtPost (
     OUT   UINT64            **Ptr,
  IN       AMD_CONFIG_PARAMS *StdHeader
  )
{
  UINT32 AddressValue;

  AddressValue = (UINT32) P_STATE_DATA_GATHER_TEMP_ADDR;

  *Ptr = (UINT64 *)(AddressValue);

  return AGESA_SUCCESS;
}


/*---------------------------------------------------------------------------------------*/
/**
 * AP task to sync memory subsystem MSRs with the BSC
 *
 * This function processes a list of MSRs and the BSC's current values for those
 * MSRs.  This will allow the APs to see system RAM.
 *
 * @param[in]  MtrrTable         Memory related MSR table
 * @param[in]  StdHeader         Config handle for library and services
 *
 */
STATIC VOID
SyncAllApMtrrToBsc (
  IN       VOID *MtrrTable,
  IN       AMD_CONFIG_PARAMS *StdHeader
  )
{
  UINT8   i;

  for (i = 0; ((BSC_AP_MSR_SYNC *) MtrrTable)[i].RegisterAddress != 0; i++) {
    LibAmdMsrWrite (((BSC_AP_MSR_SYNC *) MtrrTable)[i].RegisterAddress,
                    &((BSC_AP_MSR_SYNC *) MtrrTable)[i].RegisterValue,
                    StdHeader);
  }
}


/*---------------------------------------------------------------------------------------*/
/**
 * Creates p-state information on the heap
 *
 * This function gathers p-state information from all processors in the system,
 * determines a level set of p-states, and places that information into the
 * heap.  This heap data will be used by CreateAcpiTables to generate the
 * final _PSS and XPSS objects.
 *
 * @param[in]  StdHeader         Config handle for library and services
 *
 * @retval     AGESA_SUCCESS     No error
 * @retval     AGESA_ERROR       CPU_ERROR_PSTATE_HEAP_NOT_AVAILABLE
 */
AGESA_STATUS
PstateCreateHeapInfo (
  IN       AMD_CONFIG_PARAMS   *StdHeader
  )
{
  AGESA_STATUS            AgesaStatus;
  S_CPU_AMD_PSTATE        *PStateBufferPtr;
  ALLOCATE_HEAP_PARAMS    AllocHeapParams;
  UINT8                   *PStateBufferPtrInHeap;

  ASSERT (IsBsp (StdHeader, &AgesaStatus));

  //
  //Get proper address for gather data pool address
  //Zero P-state gather data pool
  //
  GetPstateGatherDataAddressAtPost ((UINT64 **)&PStateBufferPtr, StdHeader);
  LibAmdMemFill (PStateBufferPtr, 0, sizeof (S_CPU_AMD_PSTATE), StdHeader);

  //
  //Get all the CPUs P-States and fill the PStateBufferPtr for each core
  //
  AgesaStatus = PStateGatherData (StdHeader, PStateBufferPtr);
  if (AgesaStatus != AGESA_SUCCESS) {
    return AgesaStatus;
  }

  //
  //Do Pstate Leveling for each core if needed.
  //
  AgesaStatus = PStateLeveling (PStateBufferPtr, StdHeader);

  //
  //Create Heap and store p-state data for ACPI table in CpuLate
  //
  AllocHeapParams.RequestedBufferSize = (UINT16) (PStateBufferPtr->SizeOfBytes);
  AllocHeapParams.BufferHandle = AMD_PSTATE_DATA_BUFFER_HANDLE;
  AllocHeapParams.Persist = HEAP_SYSTEM_MEM;
  AgesaStatus = HeapAllocateBuffer (&AllocHeapParams, StdHeader);
  if (AgesaStatus == AGESA_SUCCESS) {
    //
    // Zero Buffer
    //
    PStateBufferPtrInHeap = (UINT8 *) AllocHeapParams.BufferPtr;
    LibAmdMemFill (PStateBufferPtrInHeap, 0, PStateBufferPtr->SizeOfBytes, StdHeader);
    LibAmdMemCopy  (PStateBufferPtrInHeap, PStateBufferPtr, PStateBufferPtr->SizeOfBytes, StdHeader);

  } else {
    PutEventLog (AGESA_ERROR,
                   CPU_ERROR_PSTATE_HEAP_NOT_AVAILABLE,
                 0, 0, 0, 0, StdHeader);
  }

  return AgesaStatus;
}

VOID
SyncApMsrsToBsc (
  IN OUT   BSC_AP_MSR_SYNC    *ApMsrSync,
  IN       AMD_CONFIG_PARAMS  *StdHeader
  )
{
  AP_TASK                 TaskPtr;
  UINT16                  i;
  UINT32                  BscSocket;
  UINT32                  Ignored;
  UINT32                  BscCore;
  UINT32                  Core;
  UINT32                  Socket;
  UINT32                  NumberOfSockets;
  UINT32                  NumberOfCores;
  AGESA_STATUS            IgnoredSts;

  ASSERT (IsBsp (StdHeader, &IgnoredSts));

  IdentifyCore (StdHeader, &BscSocket, &Ignored, &BscCore, &IgnoredSts);
  NumberOfSockets = GetPlatformNumberOfSockets ();

  //
  //Sync all MTRR settings with BSP
  //
  for (i = 0; ApMsrSync[i].RegisterAddress != 0; i++) {
    LibAmdMsrRead (ApMsrSync[i].RegisterAddress, &ApMsrSync[i].RegisterValue, StdHeader);
  }

  TaskPtr.FuncAddress.PfApTaskI = SyncAllApMtrrToBsc;
  TaskPtr.DataTransfer.DataSizeInDwords = (UINT16) ((((sizeof (BSC_AP_MSR_SYNC)) * i) + 4) >> 2);
  TaskPtr.ExeFlags = WAIT_FOR_CORE;
  TaskPtr.DataTransfer.DataPtr = ApMsrSync;
  TaskPtr.DataTransfer.DataTransferFlags = 0;

  for (Socket = 0; Socket < NumberOfSockets; Socket++) {
    if (GetActiveCoresInGivenSocket (Socket, &NumberOfCores, StdHeader)) {
      for (Core = 0; Core < NumberOfCores; Core++) {
        if ((Socket != BscSocket) || (Core != BscCore)) {
          ApUtilRunCodeOnSocketCore ((UINT8) Socket, (UINT8) Core, &TaskPtr, StdHeader);
        }
      }
    }
  }
}

/*---------------------------------------------------------------------------------------*/
/**
 * SyncVariableMTRR
 *
 * Sync variable MTRR
 *
 * @param[in]  StdHeader         Config handle for library and services
 *
 */
VOID
STATIC
SyncVariableMTRR (
  IN       AMD_CONFIG_PARAMS  *StdHeader
  )
{
  BSC_AP_MSR_SYNC ApMsrSync[20];

  ApMsrSync[0].RegisterAddress = SYS_CFG;
  ApMsrSync[1].RegisterAddress = TOP_MEM;
  ApMsrSync[2].RegisterAddress = TOP_MEM2;
  ApMsrSync[3].RegisterAddress = 0x200;
  ApMsrSync[4].RegisterAddress = 0x201;
  ApMsrSync[5].RegisterAddress = 0x202;
  ApMsrSync[6].RegisterAddress = 0x203;
  ApMsrSync[7].RegisterAddress = 0x204;
  ApMsrSync[8].RegisterAddress = 0x205;
  ApMsrSync[9].RegisterAddress = 0x206;
  ApMsrSync[10].RegisterAddress = 0x207;
  ApMsrSync[11].RegisterAddress = 0x208;
  ApMsrSync[12].RegisterAddress = 0x209;
  ApMsrSync[13].RegisterAddress = 0x20A;
  ApMsrSync[14].RegisterAddress = 0x20B;
  ApMsrSync[15].RegisterAddress = 0xC0010016;
  ApMsrSync[16].RegisterAddress = 0xC0010017;
  ApMsrSync[17].RegisterAddress = 0xC0010018;
  ApMsrSync[18].RegisterAddress = 0xC0010019;
  ApMsrSync[19].RegisterAddress = 0;
  SyncApMsrsToBsc (ApMsrSync, StdHeader);
}

/*---------------------------------------------------------------------------------------*/
/**
 * The function suppose to do any thing need to be done at the end of AmdInitPost.
 *
 * @param[in]  StdHeader         Config handle for library and services
 *
 * @retval     AGESA_SUCCESS
 *
 */
AGESA_STATUS
FinalizeAtPost (
  IN       AMD_CONFIG_PARAMS *StdHeader
  )
{
  //
  // Execute wbinvd to ensure heap data in cache write back to memory.
  //
  ExecuteWbinvdInstruction (StdHeader);

  return AGESA_SUCCESS;
}
/*---------------------------------------------------------------------------------------*/
/**
 * Set TSC Frequency Selection.
 *
 * This function set TSC Frequency Selection.
 *
 * @param[in]  StdHeader         Config handle for library and services
 *
 */
VOID
STATIC
SetTscFreqSel (
  IN       AMD_CONFIG_PARAMS *StdHeader
  )
{
  PSTATE_CPU_FAMILY_SERVICES *FamilyServices;

  FamilyServices = NULL;

  GetFeatureServicesOfCurrentCore (&PstateFamilyServiceTable, (CONST VOID **)&FamilyServices, StdHeader);
  if (FamilyServices != NULL) {
    FamilyServices->CpuSetTscFreqSel (FamilyServices, StdHeader);
  }

}

/*---------------------------------------------------------------------------------------*/
/**
 * Set TSC Frequency Selection to all cores.
 *
 * This function set TscFreqSel to all cores in the system.
 *
 * @param[in]  StdHeader         Config handle for library and services
 *
 */
VOID
STATIC
SetCoresTscFreqSel (
  IN       AMD_CONFIG_PARAMS   *StdHeader
  )
{
  AP_TASK                 TaskPtr;
  UINT32                  BscSocket;
  UINT32                  Ignored;
  UINT32                  BscCore;
  UINT32                  Core;
  UINT32                  Socket;
  UINT32                  NumberOfSockets;
  UINT32                  NumberOfCores;
  AGESA_STATUS            IgnoredSts;

  ASSERT (IsBsp (StdHeader, &IgnoredSts));

  IdentifyCore (StdHeader, &BscSocket, &Ignored, &BscCore, &IgnoredSts);
  NumberOfSockets = GetPlatformNumberOfSockets ();

  SetTscFreqSel (StdHeader);

  TaskPtr.FuncAddress.PfApTask = SetTscFreqSel;
  TaskPtr.ExeFlags = WAIT_FOR_CORE;
  TaskPtr.DataTransfer.DataTransferFlags = 0;
  TaskPtr.DataTransfer.DataSizeInDwords = 0;

  for (Socket = 0; Socket < NumberOfSockets; Socket++) {
    if (GetActiveCoresInGivenSocket (Socket, &NumberOfCores, StdHeader)) {
      for (Core = 0; Core < NumberOfCores; Core++) {
        if ((Socket != BscSocket) || (Core != BscCore)) {
          ApUtilRunCodeOnSocketCore ((UINT8) Socket, (UINT8) Core, &TaskPtr, StdHeader);
        }
      }
    }
  }
}