/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "TestData.h"

#define T DDS_Octet
#define TSeq Bytes

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TSeq
#undef T

#define T TestData
#define TSeq TestDataSeq
#define TINITIALIZE TestDataInitializeEx
#define TFINALIZE TestDataFinalizeEx
#define TCOPY TestDataCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean TestDataInitialize(TestData* self)
{
    return TestDataInitializeEx(self, NULL, true);
}

void TestDataFinalize(TestData* self)
{
    TestDataFinalizeEx(self, NULL, true);
}

DDS_Boolean TestDataCopy(
    TestData* dst,
    const TestData* src)
{
    return TestDataCopyEx(dst, src, NULL);
}

TestData* TestDataCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    TestData* newSample = (TestData*)ZRMalloc(pool, sizeof(TestData));
    if (newSample == NULL)
    {
        printf("malloc for TestData failed.");
        return NULL;
    }
    if (!TestDataInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        TestDataDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void TestDataDestroySample(ZRMemPool* pool, TestData* sample)
{
    if (sample == NULL) return;
    TestDataFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong TestDataGetSerializedSampleMaxSize()
{
    return 259;
}

DDS_ULong TestDataGetSerializedKeyMaxSize()
{
    return 259;
}

DDS_Long TestDataGetKeyHash(
    const TestData* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = TestDataSerializeKey(sample, cdr);
    if (ret < 0)
    {
        printf("serialize key failed.");
        *result = DDS_HANDLE_NIL_NATIVE;
        return -1;
    }
    ret = CDRSerializeGetKeyHash(cdr, result->value, true);
    if (ret < 0)
    {
        printf("get keyhash failed.");
        *result = DDS_HANDLE_NIL_NATIVE;
        return -1;
    }
    result->valid = true;
    return 0;
}

DDS_Boolean TestDataHasKey()
{
    return false;
}

TypeCodeHeader* TestDataGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = TestDataGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean TestDataInitializeEx(
    TestData* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    Bytes_initialize_ex(&self->value, pool, allocateMemory);

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void TestDataFinalizeEx(
    TestData* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    Bytes_finalize(&self->value);
    if (deletePointers)
    {
    }
}

DDS_Boolean TestDataCopyEx(
    TestData* dst,
    const TestData* src,
    ZRMemPool* pool)
{
    if (!Bytes_copy(&dst->value, &src->value))
    {
        printf("copy member value failed.");
        return false;
    }
    return true;
}

void TestDataPrintData(const TestData *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    DDS_ULong valueTmpLen = Bytes_get_length(&sample->value);
    printf("sample->value: %d\n", valueTmpLen);
    for (DDS_ULong i = 0; i < valueTmpLen; ++i)
    {
        printf("sample->value[%u]: 0x%02x\n", i, *Bytes_get_reference(&sample->value, i));
    }
    printf("\n");

}

DDS::TypeCode* TestDataGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "TestData",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct TestData typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_UCHAR);
    if (memberTc != NULL)
    {
        memberTc = factory.createSequenceTC(255, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member value TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "value",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    return s_typeCode;
}

DDS_Long TestDataSerialize(const TestData* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &(sample->value)._length, 4))
    {
        printf("serialize length of sample->value failed.");
        return -2;
    }
    if ((sample->value)._contiguousBuffer)
    {
        if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)(sample->value)._contiguousBuffer, (sample->value)._length, 1))
        {
            printf("serialize sample->value failed.");
            return -2;
        }
    }
    else
    {
        for (DDS_ULong i = 0; i < (sample->value)._length; ++i)
        {
            if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*Bytes_get_reference(&sample->value, i), 1))
            {
                printf("serialize sample->value failed.");
                return -2;
            }
        }
    }

    return 0;
}

DDS_Long TestDataDeserialize(
    TestData* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    DDS_ULong valueTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &valueTmpLen, 4))
    {
        Bytes_initialize_ex(&sample->value, pool, true);
        return 0;
    }
    if (!Bytes_ensure_length(&sample->value, valueTmpLen, valueTmpLen))
    {
        printf("Set maxiumum member sample->value failed.");
        return -3;
    }
    if (sample->value._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->value._contiguousBuffer, valueTmpLen, 1))
        {
            printf("deserialize sample->value failed.");
            return -2;
        }
    }
    else
    {
        for (DDS_ULong i = 0; i < valueTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*Bytes_get_reference(&sample->value, i), 1))
            {
                printf("deserialize sample->value failed.");
                return -2;
            }
        }
    }
    return 0;
}

DDS_ULong TestDataGetSerializedSampleSize(const TestData* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    DDS_ULong valueLen = Bytes_get_length(&sample->value);
    if (valueLen != 0)
    {
        currentAlignment += 1 * valueLen;
    }

    return currentAlignment - initialAlignment;
}

DDS_Long TestDataSerializeKey(const TestData* sample, CDRSerializer *cdr)
{
    if (TestDataSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long TestDataDeserializeKey(
    TestData* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (TestDataDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong TestDataGetSerializedKeySize(const TestData* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += TestDataGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* TestDataLoanSampleBuf(TestData* sample, DDS_Boolean takeBuffer)
{
    DDS_Char* rst = (DDS_Char*)(sample->value._contiguousBuffer);
    if (takeBuffer)
    {
        sample->value._length = 0;
        sample->value._maximum = 0;
        sample->value._contiguousBuffer = NULL;
    }
    return rst;
}

void TestDataReturnSampleBuf(DDS_Char* sampleBuf)
{
    ZRDealloc(NULL, sampleBuf);
}

DDS_Long TestDataLoanDeserialize(TestData* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen)
{
#ifdef _ZRDDS_INCLUDE_DR_NO_SERIALIZE_MODE
    DDS_Char** fragments = sampleSeq->_fixedFragments;
    DDS_Char** headers = sampleSeq->_fixedHeader;
    if (totalNum > 64)
    {
        if (sampleSeq->_variousFragments == NULL || sampleSeq->_fragmentNum < totalNum)
        {
            ZRDealloc(NULL, sampleSeq->_variousFragments);
            ZRDealloc(NULL, sampleSeq->_variousHeader);
            // 分片数量大于64，需要动态分配
            sampleSeq->_variousFragments = (DDS_Char**)ZRMalloc(NULL, totalNum * sizeof(DDS_Char*));
            sampleSeq->_variousHeader = (DDS_Char**)ZRMalloc(NULL, totalNum * sizeof(DDS_Char*));
            if (NULL == sampleSeq->_variousFragments || NULL == sampleSeq->_variousHeader)
            {
                printf("malloc for _variousFragments failed.\n");
                return -1;
            }
            memset(sampleSeq->_variousFragments, 0, sizeof(totalNum * sizeof(DDS_Char*)));
            memset(sampleSeq->_variousHeader, 0, sizeof(totalNum * sizeof(DDS_Char*)));
        }
        fragments = sampleSeq->_variousFragments;
        headers = sampleSeq->_variousHeader;
    }
    sampleSeq->_fragmentNum = totalNum;
    if (totalNum == 1)
    {
        sampleSeq->_length = *(DDS_ULong*)(base + offset + fixedHeaderLen - 4);
        sampleSeq->_maximum = sampleSeq->_length;
        sampleSeq->_firstFragSize = space - fixedHeaderLen;
        fragments[curIndex] = base + offset + fixedHeaderLen;
        headers[curIndex] = base;
    }
    else if (curIndex == 0)
    {
        sampleSeq->_length = *(DDS_ULong*)(base + offset + fixedHeaderLen - 4);
        sampleSeq->_maximum = sampleSeq->_length;
        sampleSeq->_firstFragSize = space - fixedHeaderLen;
        fragments[curIndex] = base + offset + fixedHeaderLen;
        headers[curIndex] = base;
    }
    else if (curIndex == totalNum - 1)
    {
        sampleSeq->_lastFragSize = space;
        fragments[curIndex] = base + offset;
        headers[curIndex] = base;
    }
    else
    {
        sampleSeq->_fragmentSize = space;
        fragments[curIndex] = base + offset;
        headers[curIndex] = base;
    }
#endif /* _ZRDDS_INCLUDE_DR_NO_SERIALIZE_MODE */
    return 0;
}

#endif /*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long TestDataOnSiteDeserialize(CDRDeserializer* cdr,
    TestData* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    Bytes* seqMember = &(sample->value);
    if (!Bytes_set_maximum(seqMember, totalSize - fixedHeaderLen))
    {
        printf("Set maxiumum member value failed.");
        return -3;
    }
    if (offset == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*)&seqMember->_length, 4))
        {
            printf("get value length failed.");
            return -1;
        }
        memcpy(seqMember->_contiguousBuffer,
            (DDS_Char*)payload + fixedHeaderLen,
            payloadLen - fixedHeaderLen);
        return 0;
    }
    memcpy(seqMember->_contiguousBuffer + offset - fixedHeaderLen,
        payload, payloadLen);
    return 0;
}

DDS_Boolean TestDataNoSerializingSupported()
{
    return true;
}

DDS_ULong TestDataFixedHeaderLength()
{
    DDS_ULong curLen = 0;
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    return curLen;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
