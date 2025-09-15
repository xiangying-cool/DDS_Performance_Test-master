#ifndef TestDataDataReader_h__
#define TestDataDataReader_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "TestData.h"
#include "ZRDDSDataReader.h"

typedef struct TestDataSeq TestDataSeq;

typedef DDS::ZRDDSDataReader<TestData, TestDataSeq> TestDataDataReader;

#endif

