/*
 Copyright (c) 2014, Hiroki Mori
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met: 
 
 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer. 
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution. 
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies, 
 either expressed or implied, of the FreeBSD Project. 
 */

#include <iostream>
#include <libusb.h>

#include "QMI.h"

using namespace std;

#define QMI_INTERFACE 1

#define GET_ENCAPSULATED_RESPONSE 0x01
#define SEND_ENCAPSULATED_COMMAND 0x00

//
// USB function
//

int ReadSync(
			 libusb_device_handle *    pDev,
			 void **        ppOutBuffer,
			 u16            clientID,
			 u16            transactionID )
{
	int result; //for return values
	int 	transferred;

	unsigned char *intbuf = (unsigned char *)malloc(1024);

	// wait for get interrupt transfer
	result = libusb_interrupt_transfer(pDev, 0x82, intbuf, 1024, &transferred, 0);
	if(result == 0 && transferred == 8) {
		int i;
		for(i = 0; i < transferred; ++i) {
			printf("%02x ", intbuf[i]);
		}
		printf("\n");
		cout<<"libusb_interrupt_transfer: " << result << " " << transferred <<endl;
		
		cout<<"GET_ENCAPSULATED_RESPONSE..." <<endl;
		result = libusb_control_transfer(pDev,
										 LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
										 GET_ENCAPSULATED_RESPONSE, 0, QMI_INTERFACE,
										 (unsigned char *)*ppOutBuffer, 1024, 0);
		for(i = 0; i < result; ++i) {
			printf("%02x ", ((unsigned char *)*ppOutBuffer)[i]);
		}
		printf("\n");
	} else {
		result = 0;
	}

	free(intbuf);
	return result;
}

int WriteSync(
			  libusb_device_handle *        pDev,
			  char *             pWriteBuffer,
			  int                writeBufferSize,
			  u16                clientID )
{
	int result; //for return values
	
	FillQMUX( clientID, pWriteBuffer, writeBufferSize );
	int i;
	for(i = 0; i < writeBufferSize; ++i) {
		printf("%02x ", *((unsigned char*)pWriteBuffer + i));
	}
	printf("\n");
	
	cout<<"SEND_ENCAPSULATED_COMMAND..." <<endl;
	result = libusb_control_transfer(pDev,
								LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
								SEND_ENCAPSULATED_COMMAND, 0, QMI_INTERFACE,
								(unsigned char*)pWriteBuffer, writeBufferSize, 0);
	return result;
}

//
// QMI CTL ClientID function
//

int GetClientID( 
				libusb_device_handle *    pDev,
				u8             serviceType )
{
	int i;
	int result;
	void * pWriteBuffer;
	u16 writeBufferSize;
	u8 transactionID;
	u16 clientID;
	int readBufferSize;

	transactionID = 1;
	writeBufferSize = QMICTLGetClientIDReqSize();
	pWriteBuffer = malloc( writeBufferSize );
	QMICTLGetClientIDReq( pWriteBuffer, 
						 writeBufferSize,
						 transactionID, serviceType);
	result = WriteSync( pDev,
					   (char*)pWriteBuffer,
					   writeBufferSize,
					   QMICTL );
	unsigned char *pReadBuffer = (unsigned char *)malloc(1024);
	do {
		result = ReadSync( pDev,
						  (void **)&pReadBuffer,
						  QMICTL,
						  transactionID );
		readBufferSize = result;
		
		result = QMICTLGetClientIDResp( pReadBuffer,
									   readBufferSize,
									   &clientID );
		cout<<"QMICTLGetClientIDResp: " << result << " " << clientID <<endl;
	} while (result != 0);   // wait for correct responce

	return clientID;
}

void ReleaseClientID(
					 libusb_device_handle *    pDev,
					 u16            clientID )
{
	int result;
	void * pWriteBuffer;
	u16 writeBufferSize;
	void * pReadBuffer;
	u16 readBufferSize;
	u8 transactionID;
	
	// Run QMI ReleaseClientID if this isn't QMICTL   
	if (clientID != QMICTL)
	{
		// Note: all errors are non fatal, as we always want to delete 
		//    client memory in latter part of function
		
		writeBufferSize = QMICTLReleaseClientIDReqSize();
		pWriteBuffer = malloc( writeBufferSize );
		pReadBuffer = malloc( 1024 );
		if (pWriteBuffer == NULL)
		{
//			DBG( "memory error\n" );
		}
		else
		{
			transactionID = 1;
			result = QMICTLReleaseClientIDReq( pWriteBuffer, 
											  writeBufferSize,
											  transactionID,
											  clientID );
			if (result < 0)
			{
				free( pWriteBuffer );
//				DBG( "error %d filling req buffer\n", result );
			}
			else
			{
				result = WriteSync( pDev,
								   (char *)pWriteBuffer,
								   writeBufferSize,
								   QMICTL );
				free( pWriteBuffer );
				
				if (result < 0)
				{
//					DBG( "bad write status %d\n", result );
				}
				else
				{
					result = ReadSync( pDev,
									  &pReadBuffer,
									  QMICTL,
									  transactionID );
					if (result < 0)
					{
//						DBG( "bad read status %d\n", result );
					}
					else
					{
						readBufferSize = result;
						
						result = QMICTLReleaseClientIDResp( pReadBuffer,
														   readBufferSize );
						free( pReadBuffer );
						
						if (result < 0)
						{
//							DBG( "error %d parsing response\n", result );
						}
					}
				}
			}
		}
	}
	
}

//
//　QMI NAS function
//

int QMINASGetSigInfoSize()
{
	return sizeof( sQMUX ) + 7;
}

int QMINASGetSigInfoReq(
						 void *   pBuffer,
						 u16      buffSize,
						 u16      transactionID )
{
	if (pBuffer == 0 || buffSize < QMINASGetSigInfoSize() )
	{
		return -ENOMEM;
	}
	
	// QMI NAS GET SERIAL NUMBERS REQ
	// Request
	*(u8 *)((u8 *)pBuffer + sizeof( sQMUX ))  = 0x00;
	// Transaction ID
	*(u16 *)((u8 *)pBuffer + sizeof( sQMUX ) + 1) = transactionID;
	// Message ID
	*(u16 *)((u8 *)pBuffer + sizeof( sQMUX ) + 3) = 0x004D;
	// Size of TLV's
	*(u16 *)((u8 *)pBuffer + sizeof( sQMUX ) + 5) = 0x0000;
	
	// success
	return sizeof( sQMUX ) + 7;
}

int QMINASGetSigInfoResp(
						  void *   pBuffer,
						  u16      buffSize,
						  char *   pMEID,
						  int      meidSize )
{
	int result;
	
	// Ignore QMUX and SDU
	u8 offset = sizeof( sQMUX ) + 3;
	
	if (pBuffer == 0 || buffSize < offset || meidSize < 14 )
	{
		return -ENOMEM;
	}
	
	pBuffer = (u8 *)pBuffer + offset;
	buffSize -= offset;
	
	result = GetQMIMessageID( pBuffer, buffSize );
	if (result != 0x4D)
	{
		return -EFAULT;
	}
	
	result = ValidQMIMessage( pBuffer, buffSize );
	if (result != 0)
	{
		return -EFAULT;
	}
/*
	result = GetTLV( pBuffer, buffSize, 0x13, (void*)pMEID, meidSize );
	if (result <= 0)
	{
		return -EFAULT;
	}
 */
	return result;
}

//
//
//

int QMINASGetSigInfo( libusb_device_handle * pDev )
{
	int result;
	void * pWriteBuffer;
	u16 writeBufferSize;
	void * pReadBuffer;
	u16 readBufferSize;
	u16 NASClientID;
	unsigned char mSignalValue[1024];
	
	result = GetClientID( pDev, QMINAS );
	if (result < 0)
	{
		return result;
	}
	NASClientID = result;
	
	// QMI NAS Get Serial numbers Req
	writeBufferSize = QMINASGetSigInfoSize();
	pWriteBuffer = malloc( writeBufferSize );
	if (pWriteBuffer == NULL)
	{
		return -ENOMEM;
	}
	
	result = QMINASGetSigInfoReq( pWriteBuffer, 
                              writeBufferSize,
                              1 );
	if (result < 0)
	{
		free( pWriteBuffer );
		return result;
	}
	
	result = WriteSync( pDev,
                       (char *)pWriteBuffer,
                       writeBufferSize,
                       NASClientID );
	free( pWriteBuffer );
	
	if (result < 0)
	{
		return result;
	}

	pReadBuffer = malloc( 1024 );
	// QMI NAS Get Serial numbers Resp
	result = ReadSync( pDev,
                      &pReadBuffer,
                      NASClientID,
                      1 );
	if (result < 0)
	{
		return result;
	}
	readBufferSize = result;
	memset( &mSignalValue[0], '0', 1024 );
	result = QMINASGetSigInfoResp( (char *)pReadBuffer,
                               readBufferSize,
                               (char *)&mSignalValue[0],
                               1024 );

	free( pReadBuffer );
	cout<<"QMINASGetSigInfoResp: " << result <<endl;

	if(result > 0) {
		int i;
		for(i = 0; i < result; ++i) {
			printf("%02x ", mSignalValue[i]);
		}
		printf("\n");
	}

	if (result < 0)
	{
//		DBG( "bad get MEID resp\n" );
	}
	
	ReleaseClientID( pDev, NASClientID );
	
	// Success
	return 0;
}

//
// main
//

int main() {
	libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
	libusb_device_handle *dev_handle; //a device handle
	libusb_context *ctx = NULL; //a libusb session
	int result; //for return values
	ssize_t cnt; //holding number of devices in list
	result = libusb_init(&ctx); //initialize the library for the session we just declared
	if(result < 0) {
		cout<<"Init Error "<<result<<endl; //there was an error
		return 1;
	}
	libusb_set_debug(ctx, 0);
	
	cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
	if(cnt < 0) {
		cout<<"Get Device Error"<<endl; //there was an error
		return 1;
	}
	cout<<cnt<<" Devices in list."<<endl;
	
	dev_handle = libusb_open_device_with_vid_pid(ctx, 0x0619, 0x0211); // SII WX02S
	if(dev_handle == NULL)
		cout<<"Cannot open device"<<endl;
	else
		cout<<"Device Opened"<<endl;
	libusb_free_device_list(devs, 1); //free the list, unref the devices in it

	if(libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
		cout<<"Kernel Driver Active"<<endl;
		if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
			cout<<"Kernel Driver Detached!"<<endl;
	}
	
	result = libusb_claim_interface(dev_handle, QMI_INTERFACE); //claim interface 0 (the first) of device (mine had jsut 1)
	if(result < 0) {
		cout<<"Cannot Claim Interface"<<endl;
		return 1;
	}
	cout<<"Claimed Interface"<<endl;

	result = QMINASGetSigInfo( dev_handle );
		
	result = libusb_release_interface(dev_handle, QMI_INTERFACE); //release the claimed interface
	if(result!=0) {
		cout<<"Cannot Release Interface"<<endl;
		return 1;
	}
	cout<<"Released Interface"<<endl;

	libusb_close(dev_handle); //close the device we opened
	libusb_exit(ctx); //needs to be called to end the
	
	return 0;
}