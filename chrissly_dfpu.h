//------------------------------------------------------------------------------
/**
    chrissly_dfpu.h

    Client-API for a Decimal Floating Point Unit (DFPU) prototype based on the
    teensy 4.0 board.

    Add this line:
        #define CHRISSLY_DFPU_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the implementation.

    The USB-HID code is adapted from the teensy sample application available
    here: https://www.pjrc.com/teensy/rawhid.html

    (C) 2025 Christian Bleicher
*/
//------------------------------------------------------------------------------
#ifndef INCLUDE_CHRISSLY_DFPU_H
#define INCLUDE_CHRISSLY_DFPU_H
#define CHRISSLY_DECIMAL_IMPLEMENTATION
#include <chrissly_decimal.h>

/// detects device and establishes the connection
void dfpu_init(void);
/// close connection to device and free all resources
void dfpu_term(void);

/// add packed decimal elements in a and b, and store the results in r
void dfpu_add_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[]);
/// subtract packed decimal elements in b from packed elements in a, and store the results in r
void dfpu_subtract_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[]);
/// multiply packed decimal elements in a and b, and store the results in r
void dfpu_multiply_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[]);
/// divide packed decimals in a by packed elements in b, and store the results in r
void dfpu_divide_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[]);

#endif

//------------------------------------------------------------------------------
//
// Implementation
//
//------------------------------------------------------------------------------
#ifdef CHRISSLY_DFPU_IMPLEMENTATION

enum operation
{
    op_add = 0,
    op_subtract,
    op_multiply,
    op_divide
};

static bool device_connected = false;

//------------------------------------------------------------------------------
// Windows backend
//------------------------------------------------------------------------------
#ifdef CHRISSLY_DFPU_WINDOWS
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidclass.h>

#pragma comment (lib, "hid.lib")
#pragma comment (lib, "setupapi.lib")

#define BUFFER_SIZE 64U

static HANDLE device = INVALID_HANDLE_VALUE;
static HANDLE receive_event = NULL;
static CRITICAL_SECTION receive_mutex = {0};
static HANDLE send_event = NULL;
static CRITICAL_SECTION send_mutex = {0};

static void device_close(void);

//------------------------------------------------------------------------------
static void
device_open(void)
{
    device_close();

    GUID guid;
    HidD_GetHidGuid(&guid);

    HDEVINFO info = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (info == INVALID_HANDLE_VALUE) return;

    DWORD index;
    for (index = 0U; 1; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface = {0};
        iface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        BOOL result = SetupDiEnumDeviceInterfaces(info, NULL, &guid, index, &iface);
        if (!result) return;

        DWORD reqd_size;
        SetupDiGetInterfaceDeviceDetail(info, &iface, NULL, 0, &reqd_size, NULL);
        SP_DEVICE_INTERFACE_DETAIL_DATA* details = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(reqd_size);
        if (details == NULL) continue;
        memset(details, 0, reqd_size);
        details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        result = SetupDiGetDeviceInterfaceDetail(info, &iface, details, reqd_size, NULL, NULL);
        if (!result)
        {
            free(details);
            continue;
        }

        device = CreateFile(
            details->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL
        );
        free(details);
        if (device == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrib = {0};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);
        result = HidD_GetAttributes(device, &attrib);

        PHIDP_PREPARSED_DATA hid_data = {0};
        if (!result || (attrib.VendorID != 0x16C0) || (attrib.ProductID != 0x0486) || !HidD_GetPreparsedData(device, &hid_data))
        {
            CloseHandle(device);
            continue;
        }
        HIDP_CAPS capabilities = {0};
        if (!HidP_GetCaps(hid_data, &capabilities) || (capabilities.UsagePage != 0xFFAB) || (capabilities.Usage != 0x0200))
        {
            HidD_FreePreparsedData(hid_data);
            CloseHandle(device);
            continue;
        }
        HidD_FreePreparsedData(hid_data);

        device_connected = true;

        receive_event = CreateEvent(NULL, TRUE, TRUE, NULL);
        InitializeCriticalSection(&receive_mutex);
        send_event = CreateEvent(NULL, TRUE, TRUE, NULL);
        InitializeCriticalSection(&send_mutex);

        return;
    }
}

//------------------------------------------------------------------------------
static void
device_close(void)
{
    if (device != INVALID_HANDLE_VALUE)
    {
        CloseHandle(device);
        device = INVALID_HANDLE_VALUE;
    }
    device_connected = false;

    DeleteCriticalSection(&send_mutex);
    CloseHandle(send_event);

    DeleteCriticalSection(&receive_mutex);
    CloseHandle(receive_event);
}

//------------------------------------------------------------------------------
static int
device_receive(void* buf, unsigned int len, unsigned int timeout)
{
    unsigned char tmpbuf[516U] = {0};
    OVERLAPPED ov;
    DWORD n, r;

    if (sizeof(tmpbuf) < (size_t)len + 1U || !device || !device_connected) return -1;

    EnterCriticalSection(&receive_mutex);

    ResetEvent(&receive_event);
    memset(&ov, 0, sizeof(ov));
    ov.hEvent = receive_event;
    if (!ReadFile(device, tmpbuf, len + 1U, NULL, &ov))
    {
        if (GetLastError() != ERROR_IO_PENDING) goto return_error;
        r = WaitForSingleObject(receive_event, timeout);
        if (r == WAIT_TIMEOUT) goto return_timeout;
        if (r != WAIT_OBJECT_0) goto return_error;
    }
    if (!GetOverlappedResult(device, &ov, &n, FALSE)) goto return_error;

    LeaveCriticalSection(&receive_mutex);

    if (n <= 0U) return -1;
    --n;
    if (n > len) n = len;
    memcpy(buf, tmpbuf + 1U, n);

    return n;

return_timeout:
    CancelIo(device);
    LeaveCriticalSection(&receive_mutex);
    device_connected = false;
    return 0;

return_error:
    LeaveCriticalSection(&receive_mutex);
    device_connected = false;
    return -1;
}

//------------------------------------------------------------------------------
static int
device_send(void* buf, unsigned int len, unsigned int timeout)
{
    unsigned char tmpbuf[516U] = {0};
    OVERLAPPED ov;
    DWORD n, r;

    if (sizeof(tmpbuf) < (size_t)len + 1U || !device || !device_connected) return -1;

    EnterCriticalSection(&send_mutex);

    ResetEvent(&send_event);
    memset(&ov, 0, sizeof(ov));
    ov.hEvent = send_event;
    tmpbuf[0] = 0;
    memcpy(tmpbuf + 1U, buf, len);
    if (!WriteFile(device, tmpbuf, len + 1, NULL, &ov))
    {
        if (GetLastError() != ERROR_IO_PENDING) goto return_error;
        r = WaitForSingleObject(send_event, timeout);
        if (r == WAIT_TIMEOUT) goto return_timeout;
        if (r != WAIT_OBJECT_0) goto return_error;
    }
    if (!GetOverlappedResult(device, &ov, &n, FALSE)) goto return_error;

    LeaveCriticalSection(&send_mutex);

    if (n <= 0) return -1;
    return n - 1;

return_timeout:
    CancelIo(device);
    LeaveCriticalSection(&send_mutex);
    device_connected = false;
    return 0;

return_error:
    LeaveCriticalSection(&send_mutex);
    device_connected = false;
    return -1;
}
#endif

//------------------------------------------------------------------------------
static void
dfpu_op_packed(enum operation op, decimal_t a[4U], decimal_t b[4U], decimal_t r[])
{
    if (device_connected)
    {
#ifdef CHRISSLY_DFPU_WINDOWS
        char buffer[BUFFER_SIZE] = {0};
        buffer[0U] = op;
        buffer[1U] = a[0U].integer_places; buffer[2U] = a[0U].decimal_places; memcpy(&buffer[3U], &a[0U].significand, sizeof(int32_t));
        buffer[7U] = b[0U].integer_places; buffer[8U] = b[0U].decimal_places; memcpy(&buffer[9U], &b[0U].significand, sizeof(int32_t));
        buffer[13U] = a[1U].integer_places; buffer[14U] = a[1U].decimal_places; memcpy(&buffer[15U], &a[1U].significand, sizeof(int32_t));
        buffer[19U] = b[1U].integer_places; buffer[20U] = b[1U].decimal_places; memcpy(&buffer[21U], &b[1U].significand, sizeof(int32_t));
        buffer[25U] = a[2U].integer_places; buffer[26U] = a[2U].decimal_places; memcpy(&buffer[27U], &a[2U].significand, sizeof(int32_t));
        buffer[31U] = b[2U].integer_places; buffer[32U] = b[2U].decimal_places; memcpy(&buffer[33U], &b[2U].significand, sizeof(int32_t));
        buffer[37U] = a[3U].integer_places; buffer[38U] = a[3U].decimal_places; memcpy(&buffer[39U], &a[3U].significand, sizeof(int32_t));
        buffer[43U] = b[3U].integer_places; buffer[44U] = b[3U].decimal_places; memcpy(&buffer[45U], &b[3U].significand, sizeof(int32_t));
        device_send(buffer, BUFFER_SIZE, 100U);

        device_receive(buffer, BUFFER_SIZE, 100U);
        r[0U].integer_places = buffer[0U]; r[0U].decimal_places = buffer[1U]; memcpy(&r[0U].significand, &buffer[2U], sizeof(int32_t));
        r[1U].integer_places = buffer[6U]; r[1U].decimal_places = buffer[7U]; memcpy(&r[1U].significand, &buffer[8U], sizeof(int32_t));
        r[2U].integer_places = buffer[12U]; r[2U].decimal_places = buffer[13U]; memcpy(&r[2U].significand, &buffer[14U], sizeof(int32_t));
        r[3U].integer_places = buffer[18U]; r[3U].decimal_places = buffer[19U]; memcpy(&r[3U].significand, &buffer[20U], sizeof(int32_t));
#endif
    }
    else
    {
        unsigned int i;
        switch (op)
        {
        case op_add:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_add(a[i], b[i]);
            break;
        case op_subtract:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_subtract(a[i], b[i]);
            break;
        case op_multiply:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_multiply(a[i], b[i]);
            break;
        case op_divide:
            for (i = 0U; i < 4U; ++i) r[i] = decimal_divide(a[i], b[i]);
            break;
        default:
            break;
        }
    }
}

//------------------------------------------------------------------------------
/**
*/
void
dfpu_init()
{
#ifdef CHRISSLY_DFPU_WINDOWS
    device_open();
#endif
}

//------------------------------------------------------------------------------
/**
*/
void
dfpu_term()
{
#ifdef CHRISSLY_DFPU_WINDOWS
    device_close();
#endif
}

//------------------------------------------------------------------------------
/**
*/
void
dfpu_add_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[])
{
    dfpu_op_packed(op_add, a, b, r);
}

//------------------------------------------------------------------------------
/**
*/
void
dfpu_subtract_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[])
{
    dfpu_op_packed(op_subtract, a, b, r);
}

//------------------------------------------------------------------------------
/**
*/
void
dfpu_multiply_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[])
{
    dfpu_op_packed(op_multiply, a, b, r);
}

//------------------------------------------------------------------------------
/**
*/
void
dfpu_divide_packed(decimal_t a[4U], decimal_t b[4U], decimal_t r[])
{
    dfpu_op_packed(op_divide, a, b, r);
}

#endif