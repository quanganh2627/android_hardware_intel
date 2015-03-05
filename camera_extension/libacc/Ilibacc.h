#ifndef __ILIBACC_H__
#define __ILIBACC_H__

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

typedef int status_t;
class CameraAcc
{
    // Common methods
    public:
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
};

#endif // __ILIBACC_H__
