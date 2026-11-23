#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include <vhf.h>
#include "../Report.h"

typedef struct {
    VHFHANDLE vhf;
} DEVICE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceContext)

#pragma pack(push, 1)
typedef struct {
    UCHAR id;
    USHORT buttons;
    UCHAR hat;
    SHORT x, y, rx, ry;
    UCHAR lt, rt;
} HID_REPORT;
#pragma pack(pop)

static UCHAR Descriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x0a, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x0a, 0x81, 0x02, 0x95, 0x06, 0x81, 0x03,
    0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3b, 0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01,
    0x81, 0x42, 0x75, 0x04, 0x81, 0x03, 0x65, 0x00, 0x45, 0x00,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34,
    0x16, 0x01, 0x80, 0x26, 0xff, 0x7f, 0x75, 0x10, 0x95, 0x04, 0x81, 0x02,
    0x05, 0x02, 0x09, 0xc5, 0x09, 0xc4, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x75, 0x08, 0x95, 0x02, 0x81, 0x02, 0xc0
};

static NTSTATUS Submit(WDFDEVICE device, const TanitaReport* input) {
    static const UCHAR hats[16] = {8, 0, 4, 8, 6, 7, 5, 8, 2, 1, 3, 8, 8, 8, 8, 8};
    static const USHORT bits[] = {0x1000, 0x2000, 0x4000, 0x8000, 0x0100,
                                  0x0200, 0x0020, 0x0010, 0x0040, 0x0080};
    HID_REPORT report = {0};
    HID_XFER_PACKET packet = {0};
    VHFHANDLE vhf = DeviceContext(device)->vhf;
    if (!vhf) return STATUS_DEVICE_NOT_READY;
    report.id = 1;
    report.hat = hats[input->buttons & 15];
    for (ULONG i = 0; i < RTL_NUMBER_OF(bits); ++i) {
        if (input->buttons & bits[i]) report.buttons |= (USHORT)(1u << i);
    }
    report.x = input->leftX;
    report.y = (SHORT)-input->leftY;
    report.rx = input->rightX;
    report.ry = (SHORT)-input->rightY;
    report.lt = input->leftTrigger;
    report.rt = input->rightTrigger;
    packet.reportBuffer = (PUCHAR)&report;
    packet.reportBufferLen = sizeof(report);
    packet.reportId = report.id;
    return VhfReadReportSubmit(vhf, &packet);
}

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD AddDevice;
EVT_WDF_OBJECT_CONTEXT_CLEANUP DeleteDevice;
EVT_WDF_FILE_CLEANUP CloseFeeder;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SubmitReport;

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, AddDevice);
    return WdfDriverCreate(driver, registry, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}

NTSTATUS AddDevice(WDFDRIVER driver, PWDFDEVICE_INIT init) {
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_IO_QUEUE_CONFIG queue;
    VHF_CONFIG vhfConfig;
    NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(link, L"\\DosDevices\\Global\\TanitaGamepad");
    UNREFERENCED_PARAMETER(driver);
    WdfDeviceInitSetExclusive(init, TRUE);
    WdfDeviceInitSetCharacteristics(init, FILE_DEVICE_SECURE_OPEN, FALSE);
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, WDF_NO_EVENT_CALLBACK, WDF_NO_EVENT_CALLBACK, CloseFeeder);
    WdfDeviceInitSetFileObjectConfig(init, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    attributes.ExecutionLevel = WdfExecutionLevelPassive;
    attributes.SynchronizationScope = WdfSynchronizationScopeDevice;
    attributes.EvtCleanupCallback = DeleteDevice;
    status = WdfDeviceCreate(&init, &attributes, &device);
    if (!NT_SUCCESS(status)) return status;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue, WdfIoQueueDispatchSequential);
    queue.EvtIoDeviceControl = SubmitReport;
    status = WdfIoQueueCreate(device, &queue, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) return status;
    status = WdfDeviceCreateSymbolicLink(device, &link);
    if (!NT_SUCCESS(status)) return status;
    VHF_CONFIG_INIT(&vhfConfig, WdfDeviceWdmGetDeviceObject(device), sizeof(Descriptor), Descriptor);
    status = VhfCreate(&vhfConfig, &DeviceContext(device)->vhf);
    if (!NT_SUCCESS(status)) return status;
    return VhfStart(DeviceContext(device)->vhf);
}

VOID DeleteDevice(WDFOBJECT object) {
    DEVICE_CONTEXT* context = DeviceContext(object);
    if (context->vhf) {
        VhfDelete(context->vhf, TRUE);
        context->vhf = NULL;
    }
}

VOID CloseFeeder(WDFFILEOBJECT file) {
    const TanitaReport neutral = {0};
    Submit(WdfFileObjectGetDevice(file), &neutral);
}

VOID SubmitReport(WDFQUEUE queue, WDFREQUEST request, size_t outputSize, size_t inputSize, ULONG code) {
    TanitaReport* report;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    UNREFERENCED_PARAMETER(outputSize);
    if (code == TANITA_SUBMIT_V1 && inputSize == sizeof(TanitaReport)) {
        status = WdfRequestRetrieveInputBuffer(request, sizeof(TanitaReport), (PVOID*)&report, NULL);
        if (NT_SUCCESS(status)) {
            if ((report->buttons & ~0xf3ff) || report->leftX == MINSHORT || report->leftY == MINSHORT ||
                report->rightX == MINSHORT || report->rightY == MINSHORT) {
                status = STATUS_INVALID_PARAMETER;
            } else {
                status = Submit(WdfIoQueueGetDevice(queue), report);
            }
        }
    }
    WdfRequestComplete(request, status);
}
