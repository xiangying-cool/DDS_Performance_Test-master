/*************************************************************/
/*           ���ļ��ɱ��������ɣ����������޸�                */
/*************************************************************/
#include <stdlib.h>
#include "ZRDDSTypePlugin.h"
#include "TestData.h"
#include "TestDataTypeSupport.h"
#include "TestDataDataReader.h"
#include "TestDataDataWriter.h"
#include "ZRDDSTypeSupport.cpp"


const DDS_Char* TestData_TYPENAME = "TestData";
DDSTypeSupportImpl(TestDataTypeSupport, TestData, TestData_TYPENAME);

