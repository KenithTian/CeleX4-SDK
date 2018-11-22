#include <stdio.h>
#include "BulkTransfer.h"
#ifdef __linux__
#include <semaphore.h>
#endif // __linux__

#define MAX_IMAGE_BUFFER_NUMBER 3

CPackage  image_list[MAX_IMAGE_BUFFER_NUMBER];
static CPackage*  current_package = nullptr;
static uint8_t   write_frame_index = 0;
static uint8_t   read_frame_index = 0;
static bool      bRunning = false;
#ifdef __linux__
    static sem_t     m_sem;
#else
	static HANDLE m_hEventHandle = nullptr;
#endif // __linux__


bool submit_bulk_transfer(libusb_transfer *xfr)
{
    if ( xfr )
    {
        if(libusb_submit_transfer(xfr) == LIBUSB_SUCCESS)
        {
            return true;
            // Error
        }
        libusb_free_transfer(xfr);
    }
    return false;
}

void generate_image(uint8_t *buffer,int length)
{
    if (current_package == nullptr)
    {
        current_package = &image_list[write_frame_index];
        write_frame_index++;
        if (write_frame_index >= MAX_IMAGE_BUFFER_NUMBER)
            write_frame_index = 0;
        if (write_frame_index == read_frame_index)
        {
            read_frame_index++;
            if (read_frame_index >= MAX_IMAGE_BUFFER_NUMBER)
                read_frame_index = 0;
        }
    }
    if (current_package)
        current_package->Insert(buffer + buffer[0], length - buffer[0]);
    if ( buffer[1] & 0x02 )
    {
        if (current_package)
        {
			current_package->Insert(buffer+6, 1);
            current_package->End();
            current_package = nullptr;
#ifdef __linux__
            sem_post(&m_sem);
#else
            SetEvent(m_hEventHandle);
#endif // __linux__
        }
    }
}

void callbackUSBTransferComplete(libusb_transfer *xfr)
{
    switch(xfr->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            // Success here, data transfered are inside
            // xfr->buffer
            // and the length is
            // xfr->actual_length
            generate_image(xfr->buffer,xfr->actual_length);
            //printf("xfr->actual_length= %d\r\n",xfr->actual_length);
          //  usb_alloc_bulk_transfer();
         // usb_submit_bulk_transfer();
            submit_bulk_transfer(xfr);
            break;
        case LIBUSB_TRANSFER_TIMED_OUT:
			printf("LIBUSB_TRANSFER_TIMED_OUT\r\n");
            break;
        case LIBUSB_TRANSFER_CANCELLED:
        case LIBUSB_TRANSFER_NO_DEVICE:
        case LIBUSB_TRANSFER_ERROR:
        case LIBUSB_TRANSFER_STALL:
        case LIBUSB_TRANSFER_OVERFLOW:
            // Various type of errors here
            break;
    }
}

libusb_transfer *alloc_bulk_transfer(libusb_device_handle *device_handle,uint8_t address,uint8_t *buffer)
{
    if ( device_handle )
    {
        libusb_transfer *xfr = libusb_alloc_transfer(0);
        if ( xfr )
        {
            libusb_fill_bulk_transfer(xfr,
                          device_handle,
                          address, // Endpoint ID
                          buffer,
                          MAX_ELEMENT_BUFFER_SIZE,
                          callbackUSBTransferComplete,
                          nullptr,
                          0
                          );
            if ( submit_bulk_transfer(xfr) == true )
                return xfr;
        }
    }
    return nullptr;
}

bool Init(void)
{
#ifdef __linux__
    if ( sem_init(&m_sem,0,0) == 0 )
    {
#else
    m_hEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_hEventHandle)
    {
#endif
        bRunning = true;
        return true;
    }
    return false;
}

void Exit(void)
{
    bRunning = false;
#ifdef __linux__
    sem_destroy(&m_sem);
#else
    if(m_hEventHandle)
    {
		CloseHandle(m_hEventHandle);
		m_hEventHandle = nullptr;
    }
#endif
}

bool GetPicture(vector<uint8_t> &Image)
{
	if (bRunning == true)
	{
    #ifdef __linux__
	    int Ret = sem_wait(&m_sem);
	#else
	    DWORD Ret = WaitForSingleObject(m_hEventHandle, INFINITE);
	#endif // __linux__
		if (Ret == 0)
		{
			image_list[read_frame_index].GetImage(Image);
			read_frame_index++;
			if (read_frame_index >= MAX_IMAGE_BUFFER_NUMBER)
				read_frame_index = 0;
			if (Image.size())
				return true;
		}
	}
	return false;
}

void cancel_bulk_transfer(libusb_transfer *xfr)
{
    if ( xfr )
    {
        libusb_cancel_transfer(xfr);
       // libusb_free_transfer(xfr);
    }
}

void ClearData()
{
	for (int i = 0; i < MAX_IMAGE_BUFFER_NUMBER; i++)
	{
		image_list[i].ClearData();
	}
}
