#ifndef TestDataDataReader_h__
#define TestDataDataReader_h__
/*************************************************************/
/*           ���ļ��ɱ��������ɣ����������޸�                */
/*************************************************************/

#include "TestData.h"
#include "ZRDDSDataReader.h"

typedef struct TestDataSeq TestDataSeq;

typedef DDS::ZRDDSDataReader<TestData, TestDataSeq> TestDataDataReader;

#endif

