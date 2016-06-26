#include "stdafx.h"
#include <ntstrsafe.h>

void RegTestUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS RegTestCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS RegTestDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif
ULONG MEM_TAG = 'asdf';
NTSTATUS MyQueryValueKeyTest()
{
	OBJECT_ATTRIBUTES reg_obj = {0};
	UNICODE_STRING reg_path = 
		RTL_CONSTANT_STRING(L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
	InitializeObjectAttributes(&reg_obj,&reg_path,OBJ_CASE_INSENSITIVE,NULL,NULL);
	HANDLE my_key = NULL;
	UNICODE_STRING keyName = RTL_CONSTANT_STRING(L"SystemRoot");
	KEY_VALUE_PARTIAL_INFORMATION info={0};
	PKEY_VALUE_PARTIAL_INFORMATION pActualInfo = NULL;
	ULONG actualLen=0;
	
	NTSTATUS status ;
	do 
	{
		status = ZwOpenKey(&my_key,KEY_READ | KEY_WRITE,&reg_obj);
		if(!NT_SUCCESS(status))
			break;
		status = ZwQueryValueKey(my_key,&keyName,
			KeyValuePartialInformation,&info,sizeof(info),&actualLen);
		if(!NT_SUCCESS(status) && status!=STATUS_BUFFER_OVERFLOW
							   && status!=STATUS_BUFFER_TOO_SMALL)
		{
			break;
		}
		pActualInfo = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(NonPagedPool,actualLen,MEM_TAG);
		if(pActualInfo==NULL)
			break;
		status = ZwQueryValueKey(my_key,&keyName,
			KeyValuePartialInformation,pActualInfo,actualLen,&actualLen);
		if(!NT_SUCCESS(status))
			break;

		UNICODE_STRING temp = {0};
		/*temp.Buffer = (PWCHAR)pActualInfo->Data;
		temp.MaximumLength = temp.Length = pActualInfo->DataLength;*/
		//测试了下，Data应该是以空字符结尾的，这么调用没啥问题，MaximumLength=Length+2;
		RtlInitUnicodeString(&temp,(PWCHAR)pActualInfo->Data);
		UNICODE_STRING str = RTL_CONSTANT_STRING(L"C:\\d我哈哈ddWINDOWS");

		LONG resut = RtlCompareUnicodeString(&temp,&str,TRUE);
		KdPrint(("temp Length:%d,temp MaxLength:%d \n str Length:%d,str MaxLength:%d"
			,temp.Length,temp.MaximumLength,str.Length,str.MaximumLength));

		//貌似无法直接输出中文，这里先转换一下
		ANSI_STRING astr;
		RtlUnicodeStringToAnsiString(&astr,&str,TRUE);
		if(resut==0)
		{
			KdPrint(("aa equal value :%Z",&astr));
		}else
		{
			KdPrint(("aa not equal value :%Z",&astr));
		}
		RtlFreeAnsiString(&astr);
	} while (0);
	if(my_key)
		ZwClose(my_key);
	if(pActualInfo)
		ExFreePool(pActualInfo);
	return status;
}

NTSTATUS MySetyValueKeyTest()
{
	OBJECT_ATTRIBUTES reg_obj = {0};
	UNICODE_STRING reg_path = 
		RTL_CONSTANT_STRING(L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
	InitializeObjectAttributes(&reg_obj,&reg_path,OBJ_CASE_INSENSITIVE,NULL,NULL);
	HANDLE my_key = NULL;
	UNICODE_STRING keyName = RTL_CONSTANT_STRING(L"SystemRoot");
	NTSTATUS status ;
	do 
	{
		status = ZwOpenKey(&my_key,KEY_READ | KEY_WRITE,&reg_obj);
		if(!NT_SUCCESS(status))
			break;
		PWCHAR pValue = L"这是一个测试";
		//注意这里一定要将空字符也写入，否则的话，Query返回的数据应该就是以空结尾的
		//如果空字符写入的话，返回的数据就比写入的时候多了
		status = ZwSetValueKey(my_key,&keyName,0,REG_SZ,pValue,(wcslen(pValue)+1)*sizeof(WCHAR));
		if(!NT_SUCCESS(status))
			break;
	}while(0);

	if(my_key)
		ZwClose(my_key);
	return status;

}

VOID CustomDpc(IN PKDPC Dpc,IN PVOID DefreredContext,IN PVOID SystemArg1,IN PVOID SystemArg2)
{
	KdPrint(("CustomDPC ...."));
}
//这段代码一定会蓝屏吧，这个函数调用之后，直接返回了，堆栈上的变量就直接没了。。。
//VOID TestTimer()
//{
//
//	KTIMER my_timer;
//	KeInitializeTimer(&my_timer);
//	KDPC dpc;
//	KeInitializeDpc(&dpc,CustomDpc,NULL);
//	LARGE_INTEGER due ={0};
//	due.QuadPart = -1000*1000;
//	KeSetTimer(&my_timer,due,&dpc);
//}

KTIMER g_my_timer;
KDPC   g_my_dpc;
VOID TestTimer()
{
	KeInitializeTimer(&g_my_timer);	
	KeInitializeDpc(&g_my_dpc,CustomDpc,NULL);
	LARGE_INTEGER due ={0};
	due.QuadPart = -1000*1000;
	KeSetTimer(&g_my_timer,due,&g_my_dpc);
}
/************************************************************************/
/* 外部需要调用ExFreePool来释放内存                                            */
/************************************************************************/
PWCHAR GetCurrentTime()
{
	LARGE_INTEGER snow,now;
	TIME_FIELDS now_fields;
	KeQuerySystemTime(&snow);
	ExSystemTimeToLocalTime(&snow,&now);
	RtlTimeToTimeFields(&now,&now_fields);

	PWCHAR pStr = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool,32*sizeof(WCHAR),MEM_TAG);
	if(pStr)
	{
		memset(pStr,0,32*sizeof(WCHAR));
		RtlStringCchPrintfW(pStr,32,L"aa%4d-%2d-%2d %2d-%2d-%2d",
			now_fields.Year,now_fields.Month,now_fields.Day
			,now_fields.Hour,now_fields.Minute,now_fields.Second);
	}
	return pStr;
}

VOID MyThreadProc(IN PVOID context)
{
	HANDLE processID = PsGetCurrentProcessId();
	for(int i=0;i<10;i++)
	{
		KdPrint(("\nMyThreadProc:%d",processID));
	}
	LARGE_INTEGER due ={0};
	due.QuadPart = -10*30000;
	KeDelayExecutionThread(KernelMode,FALSE,&due);
	PsTerminateSystemThread(STATUS_SUCCESS);
}
VOID TestThread()
{
	HANDLE hThread;
	NTSTATUS status = PsCreateSystemThread(&hThread,0,NULL,NULL,NULL,MyThreadProc,NULL);
	if(NT_SUCCESS(status))
	{
		KdPrint(("\nTestThread Ok"));
		ZwClose(hThread);
	}
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	UNICODE_STRING DeviceName,Win32Device;
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status;
	unsigned i;

	RtlInitUnicodeString(&DeviceName,L"\\Device\\RegTest0");
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\RegTest0");

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = RegTestDefaultHandler;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = RegTestCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = RegTestCreateClose;
	
	DriverObject->DriverUnload = RegTestUnload;
	status = IoCreateDevice(DriverObject,
							0,
							&DeviceName,
							FILE_DEVICE_UNKNOWN,
							0,
							FALSE,
							&DeviceObject);
	if (!NT_SUCCESS(status))
		return status;
	if (!DeviceObject)
		return STATUS_UNEXPECTED_IO_ERROR;

	DeviceObject->Flags |= DO_DIRECT_IO;
	DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
	status = IoCreateSymbolicLink(&Win32Device, &DeviceName);

	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	NTSTATUS res = MyQueryValueKeyTest();
	KdPrint(("status : %x",res));
	MySetyValueKeyTest();
	PWCHAR pCurrent = GetCurrentTime();
	KdPrint(("current time:%d",pCurrent));
	if(pCurrent)
	{
		KdPrint(("Current time :%wS",GetCurrentTime()));
		ExFreePool(pCurrent);
	}

	TestTimer();
	TestThread();
	return STATUS_SUCCESS;
}

void RegTestUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING Win32Device;
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\RegTest0");
	IoDeleteSymbolicLink(&Win32Device);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS RegTestCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS RegTestDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}
