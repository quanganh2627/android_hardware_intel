#ifndef __LIBACC_H__
#define __LIBACC_H__

#include <camera/Camera.h>
#include <utils/threads.h>
#include "MessageQueue.h"

using namespace android;

// For improved readability
typedef void* host_ptr;     //!< Any normal pointer. Only used here to indicate a pointer valid in the host program
typedef void* isp_ptr;      //!< A mapped host pointer gives a isp pointer, that is valid on ISP. ISP uses different virtual address space

// A preview frame
struct Frame {
    void* img_data;         //!< Raw image data
    int id;                 //!< Id for debugging data flow path
    int frameCounter;       //!< Frame counter. Reset upon preview_start
    int width;              //!< Pixel width of image
    int height;             //!< Pixel height of image
    int format;             //!< TODO not valid at preset
    int stride;             //!< Stride of the buffer
    int size;               //!< Size of img_data in bytes. For NV12 size==1.5*stride*height
};

struct fw_info {
    size_t size;
    host_ptr data;
    unsigned int fw_handle;
};

typedef void (*preview_callback)(Frame* f);

class CameraAcc: public Thread
{
    // Common methods
    public:
        // constructor/deconstructor
        CameraAcc(sp<Camera> cam);
        ~CameraAcc();

        status_t acc_read_fw(const char* filename, fw_info &fw);
        status_t acc_upload_fw_standalone(fw_info &fw);
        status_t acc_upload_fw_extension(fw_info &fw);
        status_t acc_start_standalone();
        status_t acc_wait_standalone();
        status_t acc_abort_standalone();
        status_t acc_unload_standalone();
        host_ptr host_alloc(int size);
        status_t host_free(host_ptr ptr);
        status_t acc_map(host_ptr in, isp_ptr &out);
        status_t acc_sendarg(isp_ptr arg);
        status_t acc_unmap(isp_ptr p);
        status_t register_callback(preview_callback cb);
        static bool dumpImage2File(const void* data, const unsigned int width_padded, unsigned int width,
                                   unsigned int height, const char* name);

        // Callbacks
        void notifyPointer(int32_t data, int32_t idx);
        void notifyFinished();
        void postArgumentBuffer(sp<IMemoryHeap> heap, uint8_t *heapBase, size_t size, ssize_t offset);
        void postPreviewBuffer(sp<IMemoryHeap> heap, uint8_t *heapBase, size_t size, ssize_t offset);
        void postMetadataBuffer(sp<IMemoryHeap> heap, uint8_t *heapBase, size_t size, ssize_t offset);

    // Thread overrides
    public:
        status_t requestExitAndWait();

    // private types
    private:
        // thread message id's
        enum MessageId {

            MESSAGE_ID_EXIT = 0,            // call requestExitAndWait
            MESSAGE_ID_HOST_ALLOC,
            MESSAGE_ID_MAP,
            MESSAGE_ID_WAIT_STANDALONE,

            // max number of messages
            MESSAGE_ID_MAX
        };

        struct MessageAlloc {
            int size;
        };

        struct MessageMap {
            int idx;
        };

        // union of all message data
        union MessageData {
            // MESSAGE_ID_ALLOC
            MessageAlloc alloc;
            // MESSAGE_ID_MAP
            MessageMap map;
        };

        // message id and message data
        struct Message {
            MessageId id;
            MessageData data;
        };

        // struct holding arguments for ISP
        struct ArgumentBuffer {
            sp<IMemoryHeap> mem;    // pointer to MemoryHeap object
            void* ptr;              // mapped pointer, valid on ISP
        };

    // inherited from Thread
    private:
        virtual bool threadLoop();

    // private methods
    private:
        status_t acc_upload_fw(fw_info &fw);
        status_t handleExit();
        status_t handleMessageHostAlloc(const MessageAlloc& msg);
        status_t handleMessageMap(const MessageMap& msg);
        status_t handleMessageWaitStandalone();

        // main message function
        status_t waitForAndExecuteMessage();

    // private data
    private:
        MessageQueue<Message, MessageId> mMessageQueue;
        bool mThreadRunning;

        sp<Camera> mCamera;
        bool mStandaloneMode;

        Vector<ArgumentBuffer> mArgumentBuffers;

        Frame mFrame;

        Frame* mFrameMetadata;
        sp<IMemoryHeap> mFrameMetadataBuffer;

        preview_callback mCallback;
};

#endif // __LIBACC_H__
