#ifndef __TXEI_DRV_H__
#define __TXEI_DRV_H__

#include <inttypes.h>
#include <sys/types.h>

#define MEI_DEVICE_FILE "/dev/mei"
#define MEI_VERSION_SYSFS_FILE "/sys/module/mei/version"

typedef struct guid {
	unsigned int data1;
	unsigned short data2;
	unsigned short data3;
	unsigned char data4[8];
} GUID;

typedef enum _MEI_STATUS {
	MEI_STATUS_OK = 0x0,
	MEI_STATUS_GENERAL_ERROR = 0x2000,
	MEI_STATUS_LOCATE_DEVICE_ERROR,
	MEI_STATUS_MEMORY_ACCESS_ERROR,
	MEI_STATUS_WRITE_REGISTER_ERROR,
	MEI_STATUS_MEMORY_ALLOCATION_ERROR,
	MEI_STATUS_BUFFER_OVEREFLOW_ERROR,
	MEI_STATUS_NOT_ENOUGH_MEMORY,
	MEI_STATUS_MSG_TRANSMISSION_ERROR,
	MEI_STATUS_VERSION_MISMATCH,
	MEI_STATUS_UNEXPECTED_INTERRUPT_REASON,
	MEI_STATUS_TIMEOUT_ERROR,
	MEI_STATUS_UNEXPECTED_RESPONSE,
	MEI_STATUS_UNKNOWN_MESSAGE,
	MEI_STATUS_CANNOT_FOUND_HOST_CLIENT,
	MEI_STATUS_CANNOT_FOUND_ME_CLIENT,
	MEI_STATUS_CLIENT_ALREADY_CONNECTED,
	MEI_STATUS_NO_FREE_CONNECTION,
	MEI_STATUS_ILLEGAL_PARAMETER,
	MEI_STATUS_FLOW_CONTROL_ERROR,
	MEI_STATUS_NO_MESSAGE,
	MEI_STATUS_BUFFER_TOO_LARGE,
	MEI_STATUS_BUFFER_TOO_SMALL,
	MEI_STATUS_BUFFER_NOT_EMPTY,
	NUM_OF_MEI_STATUSES
} MEI_STATUS;

#pragma pack(1)

typedef struct mei_mm_data {
	__u64 vaddr;
	__u64 paddr;
	__u64 size;
} MEI_MM_DATA;

typedef struct mei_mm_dma {
	MEI_MM_DATA data;
	void *dmabuffer;
	int fd;
} MEI_MM_DMA;

typedef struct _MEI_VERSION {
	uint8_t major;
	uint8_t minor;
	uint8_t hotfix;
	uint16_t build;
} MEI_VERSION;

typedef struct _MEI_CLIENT {
	uint32_t MaxMessageLength;
	uint8_t ProtocolVersion;
	uint8_t reserved[3];
} MEI_CLIENT;

typedef struct _MEI_HANDLE {
	int fd;
	GUID guid;
	MEI_CLIENT client_properties;
	MEI_VERSION mei_version;
} MEI_HANDLE;

typedef union _MEFWCAPS_SKU {
	uint32_t Data;
	struct {
		uint32_t Reserved:1;	//Legacy
		uint32_t Qst:1;	//QST
		uint32_t Asf:1;	//ASF2
		uint32_t Amt:1;	//AMT Professional
		uint32_t AmtFund:1;	//AMT Fundamental
		uint32_t Tpm:1;	//TPM
		uint32_t Dt:1;	//Danbury Technology
		uint32_t Fps:1;	//Fingerprint Sensor
		uint32_t HomeIT:1;	//Home IT
		uint32_t Mctp:1;	//MCTP
		uint32_t WoX:1;	//Wake on X
		uint32_t PmcPatch:1;	//PMC Patch
		uint32_t Ve:1;	//VE
		uint32_t Tdt:1;	//Theft Deterrent Technology
		uint32_t Corp:1;	//Corporate
		uint32_t Reserved2:17;
	} Fields;
} MEFWCAPS_SKU;

typedef enum _MEFWCAPS_MANAGEABILITY_SUPP {
	MEFWCAPS_MANAGEABILITY_SUPP_NONE = 0,
	MEFWCAPS_MANAGEABILITY_SUPP_AMT,
	MEFWCAPS_MANAGEABILITY_SUPP_ASF,
	MEFWCAPS_MANAGEABILITY_SUPP_CP
} MEFWCAPS_MANAGEABILITY_SUPP;

//The application has identified an internal error
#define PTSDK_STATUS_INTERNAL_ERROR  0x1000

//An ISV operation was called while the library is not
//initialized
#define PTSDK_STATUS_NOT_INITIALIZED  0x1001

//The requested library I/F is not supported by the current library
//implementation.
#define PTSDK_STATUS_LIB_VERSION_UNSUPPORTED  0x1002

//One of the parameters is invalid (usually indicates a
//NULL pointer or an invalid session handle is specified)
#define PTSDK_STATUS_INVALID_PARAM  0x1003

//The SDK could not allocate sufficient resources to complete the operation.
#define PTSDK_STATUS_RESOURCES  0x1004

//The Library has identified a HW Internal error.
#define PTSDK_STATUS_HARDWARE_ACCESS_ERROR  0x1005

//The application that sent the request message is not registered.
//Usually indicates the registration timeout has elapsed.
//The caller should reregister with the Intel AMT enabled device.
#define PTSDK_STATUS_REQUESTOR_NOT_REGISTERED  0x1006

//A network error has occurred while processing the call.
#define PTSDK_STATUS_NETWORK_ERROR  0x1007

//Specified container can not hold the requested string
#define PTSDK_STATUS_PARAM_BUFFER_TOO_SHORT  0x1008

//For Windows only.
//ISVS_InitializeCOMinThread was not called by the current thread.
#define PTSDK_STATUS_COM_NOT_INITIALIZED_IN_THREAD  0x1009

//The URL parameter was not optional in current configuration.
#define PTSDK_STATUS_URL_REQUIRED	 0x100A

//Empty response from PTHI interface
#define PTHI_STATUS_EMPTY_RESPONSE  0x4000

#pragma pack(1)

typedef struct _FWU_VERSION {
	uint16_t Major;
	uint16_t Minor;
	uint16_t Hotfix;
	uint16_t Build;
} FWU_VERSION;

typedef enum {
	FWU_GET_VERSION = 0,
	FWU_GET_VERSION_REPLY,
	FWU_START,
	FWU_START_REPLY,
	FWU_DATA,
	FWU_DATA_REPLY,
	FWU_END,
	FWU_END_REPLY,
	FWU_GET_INFO,
	FWU_GET_INFO_REPLY
} FWU_MEI_MESSAGE_TYPE;

typedef struct _ME_GET_FW_UPDATE_INFO_REQUEST {
	uint32_t MessageID;
} ME_GET_FW_UPDATE_INFO_REQUEST;

typedef struct _FWU_MSG_REPLY_HEADER {
	uint32_t MessageType;
	uint32_t Status;
} FWU_MSG_REPLY_HEADER;

typedef struct _FWU_MSG_REPLY_HEADER_V3 {
	uint8_t MessageType;
	uint32_t Status;
} FWU_MSG_REPLY_HEADER_V3;

typedef struct _FWU_GET_VERSION_MSG_REPLY {
	uint32_t MessageType;
	uint32_t Status;
	uint32_t Sku;
	uint32_t ICHVer;
	uint32_t MCHVer;
	uint32_t Vendor;
	uint32_t LastFwUpdateStatus;
	uint32_t HwSku;
	FWU_VERSION CodeVersion;
	FWU_VERSION AMTVersion;
	uint16_t EnabledUpdateInterfaces;
	uint16_t Reserved;
} FWU_GET_VERSION_MSG_REPLY;

typedef struct _FWU_GET_VERSION_MSG_REPLY_V3 {
	uint8_t MessageType;
	uint32_t Status;
	uint32_t Sku;
	uint32_t ICHVer;
	uint32_t MCHVer;
	uint32_t Vendor;
	FWU_VERSION CodeVersion;
	FWU_VERSION RcvyVersion;
	uint16_t EnabledUpdateInterfaces;
	uint32_t LastFwUpdateStatus;
	uint32_t Reserved;
} FWU_GET_VERSION_MSG_REPLY_V3;

typedef struct _FWU_GET_INFO_MSG_REPLY {
	uint32_t MessageType;
	uint32_t Status;
	FWU_VERSION MEBxVersion;
	uint32_t FlashOverridePolicy;
	uint32_t ManageabilityMode;
	uint32_t BiosBootState;
	struct {
		uint32_t CryptoFuse:1;
		uint32_t FlashProtection:1;
		uint32_t FwOverrideQualifier:2;
		uint32_t MeResetReason:2;
		uint32_t FwOverrideCounter:8;
		uint32_t reserved:18;
	} Fields;
	uint8_t BiosVersion[20];
} FWU_GET_INFO_MSG_REPLY;

#pragma pack(0)

#define IOCTL_MEI_MM_ALLOC \
	_IOWR('H' , 0x02, struct mei_mm_data)

#define IOCTL_MEI_MM_FREE \
	_IOWR('H' , 0x03, struct mei_mm_data)

/**
 * Opens the device /dev/mei, then sends ioctl to
 * establish communication with HECI client firmware
 * module with guid
 * @param[in] guid
 * 		Pointer to globally unique firmware identifier to connect to
 * @return
 * 		MEI_HANDLE which has open file ID as one of its elements
 */
MEI_HANDLE *mei_connect(const GUID * guid);

/**
 * Close connection and close the device. Handle is freed
 * and does not exist when this is done
 * @param[in] my_handle_p
 * 		Pointer to MEI_HANDLE to disconnect from
 */
void mei_disconnect(MEI_HANDLE * my_handle_p);

/**
 * Writes a buffer to SEC
 * @param[in] my_handle_p	Pointer to open MEI_HANDLE
 * @param[in] buf			Pointer to buffer to send
 * @param[in] my_size		Size of data to send
 * @return
 * 		-1 on failure, number of bytes written on success
 */
int mei_sndmsg(MEI_HANDLE * my_handle_p, void *buf, ssize_t my_size);

/**
 * Reads a buffer from SEC
 * @param[in] my_handle_p	Pointer to open MEI handle
 * @param[out] buf			Pointer to buffer to read data into
 * @param[in] my_size		Size of data to read
 * @return
 * 		-1 on failure, number of bytes read on success
 */
int mei_rcvmsg(MEI_HANDLE * my_handle_p, void *buf, ssize_t my_size);

/**
 * Wrapper which handles sending message and receiving a response in
 * a single message; also checks that the number of bytes sent and
 * received were the requested sizes. If not, an error is returned.
 * @param[in] my_handle_p	Pointer to open MEI_HANDLE
 * @param[in] snd_buf		Pointer to buffer to send
 * @param[in] snd_size		Size of data to send
 * @param[out] rcv_buf		Pointer to buffer to read data into
 * @param[in] rcv_size		Size of data to read
 * @return
 * 		-1 on failure, number of bytes read on success
 */
int mei_snd_rcv(MEI_HANDLE * my_handle_p, void *snd_buf, ssize_t snd_size,
		void *rcv_buf, ssize_t rcv_size);

/**
 * Allocate a DMA buffer
 * @param[in] my_size 		Size of buffer to allocate
 * @return
 * 		Pointer to a MEI_MM_DMA structure. The caller is responsible for
 * 		holding	onto that structure and then having it deallocated using
 * 		the mei_clear_dma API call.
 */
MEI_MM_DMA *mei_alloc_dma(ssize_t my_size);

/**
 * Frees a previously allocated DMA buffer
 * @param[in] my_dma		Pointer to MEI_MM_DMA structure to deallocate
 */
void mei_clear_dma(MEI_MM_DMA * my_dma);


#endif				/* __TXEI_DRV_H__ */
