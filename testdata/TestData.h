#ifndef TestData_H_
#define TestData_H_

/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "OsResource.h"
#include "ZRSequence.h"
#include "TypeCode.h"
#include "CDRStream.h"
#include "ZRDDSCppWrapper.h"
#include "ZRBuiltinTypes.h"

typedef struct ZRMemPool ZRMemPool;


DDS_USER_SEQUENCE_CPP(Bytes, DDS_Octet);

typedef struct TestData
{
    Bytes value; // @ID(0)
} TestData; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(TestDataSeq, TestData);

// 用户使用接口
DDS_Boolean TestDataInitialize(
    TestData* self);

DDS_Boolean TestDataInitializeEx(
    TestData* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void TestDataFinalize(
    TestData* self);

void TestDataFinalizeEx(
    TestData* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean TestDataCopy(
    TestData* dst,
    const TestData* src);

DDS_Boolean TestDataCopyEx(
    TestData* dst,
    const TestData* src,
    ZRMemPool* pool);

void TestDataPrintData(
    const TestData* sample);

DDS::TypeCode* TestDataGetTypeCode();

// 底层使用函数
TestData* TestDataCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void TestDataDestroySample(
    ZRMemPool* pool,
    TestData* sample);

DDS_ULong TestDataGetSerializedSampleMaxSize();

DDS_ULong TestDataGetSerializedSampleSize(
    const TestData* sample,
    DDS_ULong currentAlignment);

DDS_Long TestDataSerialize(
    const TestData* sample,
    CDRSerializer* cdr);

DDS_Long TestDataDeserialize(
    TestData* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong TestDataGetSerializedKeyMaxSize();

DDS_ULong TestDataGetSerializedKeySize(
    const TestData* sample,
    DDS_ULong currentAlignment);

DDS_Long TestDataSerializeKey(
    const TestData* sample,
    CDRSerializer* cdr);

DDS_Long TestDataDeserializeKey(
    TestData* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long TestDataGetKeyHash(
    const TestData* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean TestDataHasKey();

TypeCodeHeader* TestDataGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean TestDataNoSerializingSupported();

DDS_ULong TestDataFixedHeaderLength();

DDS_Long TestDataOnSiteDeserialize(CDRDeserializer* cdr,
    TestData* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* TestDataLoanSampleBuf(TestData* sample, DDS_Boolean takeBuffer);

void TestDataReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long TestDataLoanDeserialize(TestData* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
#endif
