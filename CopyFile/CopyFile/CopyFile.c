#include "stdafx.h"

void CopyFileUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS CopyFileCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS CopyFileDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

NTSTATUS MyCopyFile(IN PUNICODE_STRING src_path ,IN PUNICODE_STRING dst_path); 
NTSTATUS TestCreateFile(PUNICODE_STRING path);
#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

VOID CreateFileTest()
{
	OBJECT_ATTRIBUTES objectAttributes={0};
	IO_STATUS_BLOCK iostatus={0};
	HANDLE hfile;
	NTSTATUS ntStatus ;
	UNICODE_STRING logFileUnicodeString;
	//初始化UNICODE_STRING字符串
	RtlInitUnicodeString(&logFileUnicodeString,L"\\??\\C:\\1.log");
	//初始化objectAttributes
	InitializeObjectAttributes(&objectAttributes,
		&logFileUnicodeString,
		OBJ_CASE_INSENSITIVE,
		NULL,
		NULL );
	//创建文件
	ntStatus = ZwCreateFile( &hfile,
		GENERIC_WRITE,
		&objectAttributes,
		&iostatus,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN_IF,//即使存在该文件，也创建
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL,
		0);
	if(NT_SUCCESS(ntStatus))
	{
		KdPrint(("Create file succussfully!\n"));
	}else
	{
		KdPrint(("Create file  unsuccessfully!\n"));
	}
	if(hfile)
		ZwClose(hfile);
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	UNICODE_STRING DeviceName,Win32Device;
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status;
	unsigned i;
	UNICODE_STRING src_file = RTL_CONSTANT_STRING(L"\\??\\C:\\a.txt");
	UNICODE_STRING dst_file = RTL_CONSTANT_STRING(L"\\??\\C:\\b.txt");
	UNICODE_STRING test_file = RTL_CONSTANT_STRING(L"\\??\\C:\\c.txt");


	RtlInitUnicodeString(&DeviceName,L"\\Device\\CopyFile0");
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\CopyFile0");

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = CopyFileDefaultHandler;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = CopyFileCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = CopyFileCreateClose;
	
	DriverObject->DriverUnload = CopyFileUnload;
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

	TestCreateFile(&test_file);
	
	status = MyCopyFile(&src_file,&dst_file);
	if(NT_SUCCESS(status))
	{
		KdPrint(("mycopyfile ok :%d",status));
	}else
	{
		KdPrint(("mycopyfile error:%x",status));
	}
	CreateFileTest();
	return STATUS_SUCCESS;
}

void CopyFileUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING Win32Device;
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\CopyFile0");
	IoDeleteSymbolicLink(&Win32Device);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS CopyFileCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS CopyFileDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Irp->IoStatus.Status;
}

NTSTATUS MyCopyFile(IN PUNICODE_STRING src_path ,IN PUNICODE_STRING dst_path)
{
	HANDLE dst_file =NULL;
	HANDLE src_file = NULL;

	NTSTATUS status;
	IO_STATUS_BLOCK io_status = {0};

	PVOID buffer = NULL;
	LARGE_INTEGER offset ={0};

	ULONG length = 4*1024;
	ULONG MEM_TAG = 'ASDF';

	ULONG actualLength = 0;
	do 
	{
		OBJECT_ATTRIBUTES src_attr = {0};
		OBJECT_ATTRIBUTES dst_attr = {0};
	
		InitializeObjectAttributes(&src_attr,src_path
			,OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE
			,0,NULL);
		InitializeObjectAttributes(&dst_attr,dst_path
			,OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE
			,0,NULL);

		status = ZwCreateFile(&src_file,
			GENERIC_READ | GENERIC_WRITE,
			&src_attr,
			&io_status,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN_IF,
			FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,0);
		if(!NT_SUCCESS(status))
		{
			break;
		}

		status = ZwCreateFile(&dst_file,
			GENERIC_READ | GENERIC_WRITE,
			&dst_attr,
			&io_status,
			NULL,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN_IF,
			FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS | FILE_SYNCHRONOUS_IO_NONALERT,
			NULL,0);
		if(!NT_SUCCESS(status))
		{
			break;
		}

		buffer = ExAllocatePoolWithTag(NonPagedPool,length,MEM_TAG);
		if(!buffer)
			break;

		while(TRUE)
		{
			status = ZwReadFile(src_file,NULL,NULL,NULL,&io_status,buffer,length,&offset,NULL);
			if(!NT_SUCCESS(status))
			{
				if(status == STATUS_END_OF_FILE)
				{
					status = STATUS_SUCCESS;
				}
				break;
			}
			actualLength = io_status.Information;

			status =ZwWriteFile(dst_file,NULL,NULL,NULL,&io_status,buffer,actualLength,&offset,NULL);
			if(!NT_SUCCESS(status))
			{
				break;
			}
			offset.QuadPart = offset.QuadPart+length;
		}
	} while (0);
	
	if(src_file)
		ZwClose(src_file);
	if(dst_file)
		ZwClose(dst_file);
	if(buffer)
		ExFreePool(buffer);
	return status;
}

NTSTATUS TestCreateFile(PUNICODE_STRING path)
{

	HANDLE hFile=NULL;
	NTSTATUS status;
	IO_STATUS_BLOCK isb;
	OBJECT_ATTRIBUTES oa;

	InitializeObjectAttributes(&oa,path
						,OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE
						,NULL,NULL);

	status=ZwCreateFile(&hFile,GENERIC_ALL,
		&oa,&isb,NULL,FILE_ATTRIBUTE_NORMAL
		,FILE_SHARE_READ,FILE_CREATE
		,FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT
		,NULL,0);

	if(NT_SUCCESS(status))
	{
		status=STATUS_SUCCESS;
	}
	else
	{
		status=isb.Status;
	}

	DbgPrint("hFile=%08X",hFile);
	if(hFile)
	{
		ZwClose(hFile);
	}
	return status;
}
