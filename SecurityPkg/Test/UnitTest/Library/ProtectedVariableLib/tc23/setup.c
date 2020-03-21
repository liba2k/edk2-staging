/** @file

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>

#include <Guid/VariableFormat.h>

#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/ProtectedVariableLib.h>

#include <Library/UnitTestLib.h>

#include <Library/ProtectedVariableLib/ProtectedVariableInternal.h>

#include "TestSmmProtectedVariableLib.h"
#include "StubRpmcLib.h"
#include "StubVariableStore.h"
#include "StubFvbProtocol.h"
#include "TestStubLib.h"

#include "var_fv.c"
#include "var_data_pool.c"

/**
  Test Case:
    - Add one encryped variable
    - Update it with new data
**/

STATIC EFI_GUID  TestVar1Guid = {
  0x03EB1519,
  0xA9A0,
  0x4C43,
  {0xBD, 0x22, 0xF3, 0xAC, 0x73, 0xF1, 0x54, 0x5B}
};

STATIC EFI_GUID  TestVar2Guid = {
  0xB872A153,
  0xDC9F,
  0x41EB,
  {0x81, 0x8F, 0x00, 0xE6, 0x6D, 0x66, 0xB1, 0x7B}
};

// 04B37FE8-F6AE-480B-BDD5-37D98C5E89AA
STATIC EFI_GUID  VarErrorFlagGuid = {
  0x04b37fe8,
  0xf6ae,
  0x480b,
  {0xbd, 0xd5, 0x37, 0xd9, 0x8c, 0x5e, 0x89, 0xaa}
};

// B872A153-DC9F-41EB-818F-00E66D66B17B
STATIC EFI_GUID  TestVar3Guid = {
  0xb872a153,
  0xdc9f,
  0x41eb,
  {0x81, 0x8f, 0x00, 0xe6, 0x6d, 0x66, 0xb1, 0x7b}
};

// B872A153-DC9F-41EB-818F-00E66D66B17B
#define TestVar4Guid TestVar3Guid

// B872A153-DC9F-41EB-818F-00E66D66B17B
#define TestVar5Guid TestVar3Guid

// E3E890AD-5B67-466E-904F-94CA7E9376BB
STATIC EFI_GUID  MetaDataHmacVarGuid = {
  0xe3e890ad,
  0x5b67,
  0x466e,
  {0x90, 0x4f, 0x94, 0xca, 0x7e, 0x93, 0x76, 0xbb}
};

EFI_STATUS
EFIAPI
GetVariableInfoPei (
  IN      VARIABLE_STORE_HEADER     *VariableStore,
  IN OUT  PROTECTED_VARIABLE_INFO   *VariableInfo
  );

EFI_STATUS
EFIAPI
GetVariableInfo (
  IN      VARIABLE_STORE_HEADER     *VariableStore,
  IN OUT  PROTECTED_VARIABLE_INFO   *VariableInfo
  );

EFI_STATUS
EFIAPI
GetNextVariableInfoPei (
  IN      VARIABLE_STORE_HEADER     *VariableStore,
  IN      VARIABLE_HEADER           *VariableStart OPTIONAL,
  IN      VARIABLE_HEADER           *VariableEnd OPTIONAL,
  IN  OUT PROTECTED_VARIABLE_INFO   *VariableInfo
  );

EFI_STATUS
EFIAPI
GetNextVariableInfo (
  IN      VARIABLE_STORE_HEADER     *VariableStore,
  IN      VARIABLE_HEADER           *VariableStart OPTIONAL,
  IN      VARIABLE_HEADER           *VariableEnd OPTIONAL,
  IN  OUT PROTECTED_VARIABLE_INFO   *VariableInfo
  );

EFI_STATUS
EFIAPI
InitNvVariableStorePei (
     OUT  EFI_PHYSICAL_ADDRESS              StoreCacheBase OPTIONAL,
  IN OUT  UINT32                            *StoreCacheSize,
     OUT  UINT32                            *IndexTable OPTIONAL,
     OUT  UINT32                            *VariableNumber OPTIONAL,
     OUT  BOOLEAN                           *AuthFlag OPTIONAL
  );

EFI_STATUS
EFIAPI
ProtectedVariableLibInitializePei (
  IN  PROTECTED_VARIABLE_CONTEXT_IN   *ContextIn
  );

EFI_STATUS
EFIAPI
SmmVariableSetVariable (
  IN CHAR16                  *VariableName,
  IN EFI_GUID                *VendorGuid,
  IN UINT32                  Attributes,
  IN UINTN                   DataSize,
  IN VOID                    *Data
  );

EFI_STATUS
VerifyMetaDataHmac (
  IN      PROTECTED_VARIABLE_CONTEXT_IN   *ContextIn,
  IN OUT  PROTECTED_VARIABLE_GLOBAL       *Global
  );

STATIC STUB_INFO   mStub1 = {
  (void *)GetVariableStore,
  (void *)Stub_GetVariableStore,
  {0xcc}
};

STATIC STUB_INFO   mStub2 = {
  (void *)GetNvVariableStore,
  (void *)Stub_GetNvVariableStore,
  {0xcc}
};

UNIT_TEST_STATUS
EFIAPI
TC23_Setup (
  UNIT_TEST_CONTEXT           Context
  )
{
  PROTECTED_VARIABLE_CONTEXT_IN   ContextIn;
  EFI_STATUS                      Status;

  mCounter = 0x77;
  gIvec = tc23_ivec;

  mVariableFv = AllocatePool ((UINTN)((EFI_FIRMWARE_VOLUME_HEADER *)tc23_var_fv)->FvLength);
  if (mVariableFv == NULL) {
    return UNIT_TEST_ERROR_PREREQUISITE_NOT_MET;
  }
  CopyMem (mVariableFv, tc23_var_fv, sizeof (tc23_var_fv));

  STUB_FUNC (&mStub1);
  STUB_FUNC (&mStub2);

  ContextIn.StructSize = sizeof (ContextIn);
  ContextIn.StructVersion = 1;

  ContextIn.FindVariableSmm     = NULL;
  ContextIn.GetVariableInfo     = NULL;
  ContextIn.IsUserVariable      = NULL;
  ContextIn.UpdateVariableStore = NULL;

  ContextIn.VariableServiceUser = FromPeiModule;
  ContextIn.GetNextVariableInfo = GetNextVariableInfoPei;
  ContextIn.InitVariableStore   = InitNvVariableStorePei;
  ContextIn.GetVariableInfo     = GetVariableInfoPei;

  //
  // Use PEI code to initialize encrytped variable HOB
  //
  Status = ProtectedVariableLibInitializePei (&ContextIn);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  return UNIT_TEST_PASSED;
}

UNIT_TEST_STATUS
EFIAPI
TC23_TearDown (
  UNIT_TEST_CONTEXT           Context
  )
{
  mCounter = 0x77;

  if (mVariableFv != NULL) {
    FreePool (mVariableFv);
    mVariableFv = NULL;
  }

  RESET_FUNC (&mStub1);
  RESET_FUNC (&mStub2);

  return UNIT_TEST_PASSED;
}

STATIC
VOID
GetRandVar (
  UINT8       *Buffer,
  CHAR16      **Name,
  UINTN       *NameSize,
  UINT8       **Data,
  UINTN       *DataSize,
  UINTN       Factor
  )
{
  STATIC UINTN          PoolIndex = 0;
  STATIC UINTN          Scramble = 0;
  UINTN                 Index;
  CHAR16                NameChar;
  UINTN                 NameLength;

  *Name = (CHAR16 *)Buffer;
  do {
    NameLength = tc23_data_pool[PoolIndex] & 0xF;
    PoolIndex = (PoolIndex + 1) % sizeof (tc23_data_pool);
  } while (NameLength < 2);

  PoolIndex = (PoolIndex + Scramble) % sizeof (tc23_data_pool);
  for (Index = 0; Index < NameLength; ++Index) {
    PoolIndex = (PoolIndex + Index) % sizeof (tc23_data_pool);
    NameChar = tc23_data_pool[PoolIndex] & 0x7F;
    NameChar += (NameChar <= 0x20) ? 0x20 : 0;
    *(UINT16 *)Buffer = NameChar;
    Buffer += sizeof (UINT16);
  }

  *(UINT16 *)Buffer = 0;
  Buffer += sizeof (UINT16);

  NameLength += 1;
  *NameSize = NameLength * sizeof (UINT16);

  Buffer += GET_PAD_SIZE (*NameSize);
  *Data = Buffer;
  do {
    *DataSize = tc23_data_pool[PoolIndex];
    PoolIndex = (PoolIndex + 1) % sizeof (tc23_data_pool);
  } while (*DataSize == 0);

  PoolIndex = (PoolIndex + Scramble) % sizeof (tc23_data_pool);
  for (Index = 0; Index < *DataSize; ++Index) {
    PoolIndex = (PoolIndex + Index) % sizeof (tc23_data_pool);
    *Buffer = tc23_data_pool[PoolIndex];
    Buffer += 1;
  }

  PoolIndex = (PoolIndex + Scramble) % sizeof (tc23_data_pool);
  Scramble += Factor;
}

STATIC
UNIT_TEST_STATUS
VerifyVariableStorages (
  IN  UNIT_TEST_CONTEXT              Context,
  IN  PROTECTED_VARIABLE_CONTEXT_IN *ContextIn,
  IN  VARIABLE_STORE_HEADER         **VariableStore,
  IN  UINTN                         StoreNumber
  )
{
  EFI_STATUS                    Status;
  UINTN                         Index;
  PROTECTED_VARIABLE_INFO       VariableN0;

  for (Index = 1; Index < StoreNumber; ++Index) {
    UT_ASSERT_EQUAL (VariableStore[Index]->Size, VariableStore[0]->Size);
  }

  VariableN0.Address = NULL;
  VariableN0.Offset  = 0;
  while (TRUE) {
    Status = ContextIn->GetNextVariableInfo (VariableStore[0], NULL, NULL, &VariableN0);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (VariableN0.Address->State != VAR_ADDED) {
      continue;
    }

    UT_ASSERT_NOT_EQUAL (VariableN0.Offset, 0);

    for (Index = 1; Index < StoreNumber; ++Index) {
      DEBUG ((DEBUG_INFO, "             Checking store=%d, offset=%04x\r\n", Index, VariableN0.Offset));
      UT_ASSERT_MEM_EQUAL (
        (VOID *)VariableN0.Address,
        (VOID *)((UINTN)VariableStore[Index] + VariableN0.Offset),
        VARIABLE_SIZE (&VariableN0)
        );
    }
  }

  DEBUG ((DEBUG_INFO, "            -------------------------------\r\n", Index, VariableN0.Offset));

  return UNIT_TEST_PASSED;
}

STATIC UINT8  mBuffer[0x400];
STATIC UINT8  Variable[0x400];                             

UNIT_TEST_STATUS
EFIAPI
TC23_DoVariableStorageReclaim (
  IN UNIT_TEST_CONTEXT           Context
  )
{
  EFI_STATUS                          Status;
  PROTECTED_VARIABLE_CONTEXT_IN       ContextIn;
  PROTECTED_VARIABLE_GLOBAL           *Global;
  EFI_FIRMWARE_VOLUME_HEADER          *VarFv;
  UINTN                               NameSize;
  UINTN                               DataSize;
  UINTN                               VarSize;
  //VARIABLE_STORE_HEADER               *VarStore[3];
  UINTN                               Index;
  CHAR16                              *Name;
  UINT8                               *Data;                             
  UINT8                               *ReadData;                             
  UINTN                               ReadSize;
  UINT32                              Attr;
  VARIABLE_STORE_HEADER               *VarStore[3];
  EFI_PHYSICAL_ADDRESS                Addr;

  ContextIn.StructSize = sizeof (ContextIn);
  ContextIn.StructVersion = 1;
  ContextIn.MaxVariableSize = (UINT32)GetMaxVariableSize ();

  ContextIn.FindVariableSmm     = NULL;
  ContextIn.GetVariableInfo     = GetVariableInfo;
  ContextIn.IsUserVariable      = IsUserVariable;
  ContextIn.UpdateVariableStore = VariableExLibUpdateNvVariable;

  ContextIn.VariableServiceUser = FromSmmModule;
  ContextIn.GetNextVariableInfo = GetNextVariableInfo;
  ContextIn.InitVariableStore   = NULL;

  Status = ProtectedVariableLibInitialize (&ContextIn);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  Status = GetProtectedVariableContext (NULL, &Global);
  UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

  Stub_MmInitialize ();
  Stub_FvbInitialize ((EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)Global->ProtectedVariableCache);

  VariableCommonInitialize ();
  UT_ASSERT_NOT_EQUAL (mVariableModuleGlobal, NULL);
  mVariableModuleGlobal->FvbInstance = &gStubFvb;
  gStubFvb.GetPhysicalAddress(&gStubFvb, &Addr);
  VarFv = (EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)Addr;
  mVariableModuleGlobal->VariableGlobal.NonVolatileVariableBase = Addr + VarFv->HeaderLength;

  FixupHmacVariable ();

#if 0
  ReadData = NULL;
  ReadSize = 0;
  Attr = 0;
  // test code
  VarSize = 0;
  while (VarSize < 0x40000) {
    DEBUG ((DEBUG_INFO, "%08x\r\n", VarSize));

    SetMem (mBuffer, sizeof (mBuffer), 0xff);
    GetRandVar (mBuffer, &Name, &NameSize, &Data, &DataSize, 0xFF);
    DEBUG ((DEBUG_INFO, "Name: (%d) %s\r\n", NameSize, mBuffer));

    NameSize += GET_PAD_SIZE (NameSize);
    for (Index = 0; Index < DataSize; ++Index) {
      if (Index == 0) {
        DEBUG ((DEBUG_INFO, "Data: "));
      }

      DEBUG ((DEBUG_INFO, "%02x ", mBuffer[Index + NameSize]));
      if (((Index + 1) % 16) == 0) {
        DEBUG ((DEBUG_INFO, "\r\n      "));
      }
    }

    DEBUG ((DEBUG_INFO, "\r\n"));
    DataSize += GET_PAD_SIZE (DataSize);
    VarSize += (NameSize + DataSize);
  }

#else

  VarSize = 0;
  Index = 0;
  while (VarSize < Global->ProtectedVariableCacheSize) {
    SetMem (mBuffer, sizeof (mBuffer), 0xff);
    GetRandVar (mBuffer, &Name, &NameSize, &Data, &DataSize, 0x1000);
    DEBUG ((DEBUG_INFO, "> Set variable \"%s\"\r\n", Name));

    Status = SmmVariableSetVariable (
               Name,
               &gEfiVariableGuid,
               VARIABLE_ATTRIBUTE_NV_BS_RT,
               DataSize,
               Data
               );
    UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
    UT_ASSERT_EQUAL (mCounter, 0x7a + Index);
    ++Index;

    // Verify all valid variables
    Status = VerifyMetaDataHmac (&ContextIn, Global);
    UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

    ReadData = NULL;
    ReadSize = 0;
    Attr = 0;
#if 0
    // Get variable size
    Status = VariableServiceGetVariable (
               Name,
               &gEfiVariableGuid,
               &Attr,
               &ReadSize,
               ReadData
               );
    UT_ASSERT_EQUAL (Status, EFI_BUFFER_TOO_SMALL);
    UT_ASSERT_EQUAL (ReadSize, DataSize);

    // Get variable data
    ZeroMem (Variable, sizeof (Variable));
    ReadData = Variable;
    Status = VariableServiceGetVariable (
               Name,
               &gEfiVariableGuid,
               &Attr,
               &ReadSize,
               ReadData
               );
    UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
    UT_ASSERT_EQUAL (ReadSize, DataSize);
    UT_ASSERT_MEM_EQUAL (Data, ReadData, ReadSize);
#else
    // Makre sure all copies of variable storage are in sync.
    GetVariableStoreCache (Global, NULL, &VarStore[0], NULL, NULL);
    VarStore[1] = (VARIABLE_STORE_HEADER *)mNvVariableCache;
    VarStore[2] = (VARIABLE_STORE_HEADER *)(UINTN)mVariableModuleGlobal->VariableGlobal.NonVolatileVariableBase;
    VerifyVariableStorages (Context, &ContextIn, (VARIABLE_STORE_HEADER **)&VarStore, 3);
#endif
    VarSize += NameSize + DataSize;
  }

#endif
////////////////////////////////////////////////////////////////////////////////
/// Delete all variables 
///
  {
    PROTECTED_VARIABLE_INFO       VarInfo;
    UINT32                        Rpmc;

    GetVariableStoreCache (Global, NULL, &VarStore[0], NULL, NULL);
    Rpmc = mCounter;

    //
    // Due to MetaDataHmacVar, deleting a variable might also cause Reclaim.
    // Let's find a valid variable always from the first one.
    //
    VarInfo.Address = NULL;
    VarInfo.Offset  = 0;
    while (TRUE) {
      Status = ContextIn.GetNextVariableInfo (VarStore[0], NULL, NULL, &VarInfo);
      if (EFI_ERROR (Status)) {
        break;
      }

      //
      // Find a valid variable.
      //
      while (VarInfo.Address->State != VAR_ADDED) {
        Status = ContextIn.GetNextVariableInfo (VarStore[0], NULL, NULL, &VarInfo);
        if (EFI_ERROR (Status)) {
          break;
        }
      }

      if (EFI_ERROR (Status)) {
        break;
      }

      DEBUG ((DEBUG_VERBOSE, ">> Deleting \"%s\"\n", VarInfo.Header.VariableName));
      Status = SmmVariableSetVariable (
                 VarInfo.Header.VariableName,
                 VarInfo.Header.VendorGuid,
                 0,
                 0,
                 NULL
                 );

      if (CompareGuid (&MetaDataHmacVarGuid, VarInfo.Header.VendorGuid)) {
        UT_ASSERT_EQUAL (Status, EFI_ACCESS_DENIED);
        continue;
      } else {
        UT_ASSERT_EQUAL (Status, EFI_SUCCESS);
        if (!CompareGuid (&VarErrorFlagGuid, VarInfo.Header.VendorGuid)) {
          UT_ASSERT_NOT_EQUAL (Rpmc, mCounter);
          ++Rpmc;
        }
        UT_ASSERT_EQUAL (Rpmc, mCounter);
      }

      // Verify all valid variables
      Status = VerifyMetaDataHmac (&ContextIn, Global);
      UT_ASSERT_EQUAL (Status, EFI_SUCCESS);

      // Search next valid variable from start in case of reclaim, which will
      // break current storage layout.
      VarInfo.Address = NULL;
      VarInfo.Offset  = 0;
    }

  }

  return UNIT_TEST_PASSED;
}

