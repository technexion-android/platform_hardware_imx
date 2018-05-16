#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <CL/opencl.h>

#include "opencl-2d.h"

#define LOG_TAG "opencl-2d"

#ifdef BUILD_FOR_ANDROID
#include <cutils/log.h>
#define g2d_printf ALOGI
#else
#define g2d_printf printf
#endif

#define CL_ATOMIC_COPY_SIZE 64
#define CL_ATOMIC_COPY_MASK 0x3f
#define MAX_PATH 128
#define CL_FILE_PATH "/vendor/etc/"
#define YUYV_CONVERT_SOURCE "cl_g2d.cl"
#define YUYV_CONVERT_BIN "cl_g2d.bin"
#define YUYV_TO_NV12_KERNEL "g2d_yuyv_to_nv12"
#define YUYV_TO_YUYV_KERNEL "g2d_yuyv_to_yuyv"
#define MEM_COPY_KERNEL "g2d_mem_copy"

/*Assume max buffer are 4 buffers to be handle */
#define MAX_CL_MEM_COUNT 4

typedef enum {
    /*cached and non-continuous buffer*/
    YUYV_TO_NV12_INDEX   = 0,
    /* Uncached physical continuous buffer*/
    YUYV_TO_YUYV_INDEX   = 1,
    MEM_COPY_INDEX       = 2,
    /*Assume max kernel function to handle 2D convert */
    MAX_CL_KERNEL_COUNT  = 3
} cl_kernel_index;

static const char * kernel_name_list[MAX_CL_KERNEL_COUNT + 1] = {
    YUYV_TO_NV12_KERNEL,
    YUYV_TO_YUYV_KERNEL,
    MEM_COPY_KERNEL,
    NULL,
};

struct g2dContext {
    cl_context context;
    cl_device_id device;
    cl_program program;
    cl_command_queue commandQueue;
    cl_kernel kernel[MAX_CL_KERNEL_COUNT];
    cl_mem memInObjects[MAX_CL_KERNEL_COUNT][MAX_CL_MEM_COUNT];
    cl_mem memOutObjects[MAX_CL_KERNEL_COUNT][MAX_CL_MEM_COUNT];
    struct cl_g2d_surface *dst[MAX_CL_KERNEL_COUNT];
};

static int g2d_get_planebpp(unsigned int format, int plane);
static int g2d_get_planecount(unsigned int format);
static int g2d_get_planesize(struct cl_g2d_surface *surface, int plane);
static bool CreateMemObjects(struct g2dContext *gcontext, struct cl_g2d_surface *surface,
        bool isInput);
static bool ReadOutMemObjects(struct g2dContext *gcontext);
static cl_program CreateProgram(cl_context context, cl_device_id device,
        const char* fileName);
static cl_command_queue CreateCommandQueue(cl_context context,
        cl_device_id *device);
static cl_context CreateContext();

static int g2d_get_planebpp(unsigned int format, int plane)
{
    if(plane >= g2d_get_planecount(format))
        return 0;
	switch(format) {
	case CL_G2D_RGB565:
		return 16;
	case CL_G2D_BGRX8888:
	case CL_G2D_BGRA8888:
	case CL_G2D_RGBA8888:
	case CL_G2D_RGBX8888:
	case CL_G2D_ARGB8888:
	case CL_G2D_XRGB8888:
	case CL_G2D_ABGR8888:
	case CL_G2D_XBGR8888:
		return 32;
	case CL_G2D_UYVY:
	case CL_G2D_YUYV:
	case CL_G2D_VYUY:
	case CL_G2D_YVYU:
		return 16;
	/* for the multi-plane format,
	 * only return the bits number
	 * for Y plane
	 */
	case CL_G2D_NV12:
	case CL_G2D_NV21:
        if(plane == 0)
		    return 8;
        else
            return 4;

	case CL_G2D_YV12:
	case CL_G2D_I420:
        if(plane == 0)
		    return 8;
        else
            return 2;

	default:
		g2d_printf("%s: unsupported format for getting bpp\n", __func__);
	}
	return 0;
}

static int g2d_get_planecount(unsigned int format)
{
    switch(format) {
	case CL_G2D_RGB565:
	case CL_G2D_BGRX8888:
	case CL_G2D_BGRA8888:
	case CL_G2D_RGBA8888:
	case CL_G2D_RGBX8888:
	case CL_G2D_ARGB8888:
	case CL_G2D_XRGB8888:
	case CL_G2D_ABGR8888:
	case CL_G2D_XBGR8888:
	case CL_G2D_UYVY:
	case CL_G2D_YUYV:
	case CL_G2D_VYUY:
	case CL_G2D_YVYU:
		return 1;
	/* for the multi-plane format,
	 * only return the bits number
	 * for Y plane
	 */
	case CL_G2D_NV12:
	case CL_G2D_NV21:
		return 2;
	case CL_G2D_YV12:
	case CL_G2D_I420:
		return 3;
	default:
		g2d_printf("%s: unsupported format for getting plane count\n", __func__);
	}
	return 0;
}

static int g2d_get_planesize(struct cl_g2d_surface *surface, int plane)
{
    int bpp = g2d_get_planebpp(surface->format, plane);
    if(plane >= g2d_get_planecount(surface->format))
        return 0;

    return surface->stride * surface->height * bpp / 8;
}

static bool CreateMemObjects(struct g2dContext *gcontext, struct cl_g2d_surface *surface,
        bool isInput, int kernel_index)
{
    cl_mem memObject;
    int plane = g2d_get_planecount(surface->format);
    cl_mem_flags d_flags;
    if (isInput)
        d_flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;
    else
        d_flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;

    if (surface->usage == CL_G2D_DEVICE_MEMORY)
        d_flags |= CL_MEM_USE_UNCACHED_HOST_MEMORY_VIV;

    for (int i = 0; i < plane; i++) {
        int len = g2d_get_planesize(surface, i);

        if ((unsigned char *)surface->planes[i]== NULL) {
            g2d_printf( "Error creating memory objects on null buffer.\n");
            return false;
        }

        memObject = clCreateBuffer(gcontext->context, d_flags,
            len, (unsigned char *)surface->planes[i],
            NULL);
        if (memObject == NULL)
        {
            g2d_printf( "Error creating memory objects.\n");
            return false;
        }
        if (isInput){
            gcontext->memInObjects[kernel_index][i] = memObject;
        } else {
            gcontext->memOutObjects[kernel_index][i] = memObject;
        }
    }
    return true;
}

static bool ReadOutMemObjects(struct g2dContext *gContext)
{
    cl_int errNum;
    cl_mem memObject;
    unsigned char *buf;

    for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++){
        // Read the output buffer back to the Host
        // Since the memobj is created as CL_MEM_USE_UNCACHED_HOST_MEMORY_VIV
        // No need to read output buffer
        if ((gContext->dst[i] == NULL) ||
            (gContext->dst[i]->usage == CL_G2D_DEVICE_MEMORY))
            continue;

        int plane = g2d_get_planecount(gContext->dst[i]->format);
        for(int j = 0; j < MAX_CL_MEM_COUNT; j ++) {
            buf = (unsigned char *)gContext->dst[i]->planes[j];
            int len = g2d_get_planesize(gContext->dst[i], j);
            if ((gContext->memOutObjects[i][j] != NULL)&&
               (buf != NULL))
            {
                g2d_printf("reading result buffer plane %d.\n", i);
                errNum = clEnqueueReadBuffer(gContext->commandQueue, gContext->memOutObjects[i][j],
                CL_TRUE, 0, len,
                buf, 0, NULL, NULL);
                if (errNum != CL_SUCCESS)
                {
                    g2d_printf("Error reading result buffer plane %d.\n", i);
                    return false;
                }
            }
        }
    }

    return true;
}

static cl_program CreateProgramFromBinary(cl_context context, cl_device_id device,
        const char* fileName)
{
    FILE *fp = fopen(fileName, "rb");
    if (fp == NULL) {
        g2d_printf("Error read program binary %s.", fileName);
        return NULL;
    }

    // Determine the size of the binary
    size_t binarySize;
    fseek(fp, 0, SEEK_END);
    binarySize = ftell(fp);
    rewind(fp);
    // Load binary from disk
    unsigned char *programBinary = new unsigned char[binarySize];
    fread(programBinary, 1, binarySize, fp);
    fclose(fp);
    cl_int errNum = 0;
    cl_program program;
    cl_int binaryStatus;

    program = clCreateProgramWithBinary(context,
            1,
            &device,
            &binarySize,
            (const unsigned char**)&programBinary,
            &binaryStatus,
            &errNum);
    delete [] programBinary;
    if (errNum != CL_SUCCESS)
    {
        g2d_printf("Error loading program binary.");
        return NULL;
    }
    if (binaryStatus != CL_SUCCESS)
    {
        g2d_printf("Invalid binary for device");
        return NULL;
    }
    errNum = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        // Determine the reason for the error
        char buildLog[16384];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                sizeof(buildLog), buildLog, NULL);
        g2d_printf("Error in program: ");
        g2d_printf("%s", buildLog);
        clReleaseProgram(program);
        return NULL;
    }
    return program;
}

static cl_program CreateProgram(cl_context context, cl_device_id device,
        const char* fileName)
{
    cl_int errNum;
    cl_program program;
    size_t program_length;
    FILE* pFileStream = NULL;

    pFileStream = fopen(fileName, "rb");
    if(pFileStream == 0)
    {
       g2d_printf("Failed to open file %s for reading\n" ,fileName);
       return NULL;
    }

    // get the length of the source code
    fseek(pFileStream, 0, SEEK_END);
    program_length = ftell(pFileStream);
    fseek(pFileStream, 0, SEEK_SET);

    // allocate a buffer for the source code string and read it in
    char* source = (char *)malloc(program_length + 1);
    if (fread((source), program_length, 1, pFileStream) != 1)
    {
       fclose(pFileStream);
       free(source);
       g2d_printf("Failed to open file %s for reading\n" ,fileName);
       return NULL;
    }
    fclose(pFileStream);
    source[program_length] = '\0';

    program = clCreateProgramWithSource(context, 1,
            (const char**)&source,
            NULL, NULL);
    free(source);
    if (program == NULL)
    {
        g2d_printf("Failed to create CL program from source.\n");
        return NULL;
    }
    errNum = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        // Determine the reason for the error
        char buildLog[16384];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG,
                sizeof(buildLog), buildLog, NULL);
        g2d_printf("Error in build kernel:\n");
        g2d_printf("%s", buildLog);
        clReleaseProgram(program);
        return NULL;
    }
    return program;
}

static cl_command_queue CreateCommandQueue(cl_context context,
        cl_device_id *device)
{
    cl_int errNum;
    cl_device_id *devices;
    cl_command_queue commandQueue = NULL;
    size_t deviceBufferSize = -1;
    // First get the size of the devices buffer
    errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, NULL,
            &deviceBufferSize);
    if (errNum != CL_SUCCESS)
    {
        g2d_printf("Failed call to clGetContextInfo(...,GL_CONTEXT_DEVICES,...)\n");
        return NULL;
    }
    if (deviceBufferSize <= 0)
    {
        g2d_printf("No devices available.\n");
        return NULL;
    }
    // Allocate memory for the devices buffer
    devices = (cl_device_id *)malloc(deviceBufferSize);
    errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES,
            deviceBufferSize, devices, NULL);
    if (errNum != CL_SUCCESS)
    {
        g2d_printf("Failed to get device IDs\n");
        return NULL;
    }

    commandQueue = clCreateCommandQueue(context,
            devices[0], 0, NULL);
    if (commandQueue == NULL)
    {
        g2d_printf("Failed to create commandQueue for device 0\n");
        return NULL;
    }
    *device = devices[0];
    free(devices);
    return commandQueue;
}

static cl_context CreateContext()
{
    cl_int errNum;
    cl_uint numPlatforms;
    cl_platform_id firstPlatformId;
    cl_context context = NULL;

    errNum = clGetPlatformIDs(1, &firstPlatformId, &numPlatforms);
    if (errNum != CL_SUCCESS || numPlatforms <= 0)
    {
        g2d_printf("Failed to find any OpenCL platforms.\n");
        return NULL;
    }

    cl_context_properties contextProperties[] =
    {
        CL_CONTEXT_PLATFORM,
        (cl_context_properties)firstPlatformId,
        0
    };
    context = clCreateContextFromType(contextProperties,
            CL_DEVICE_TYPE_GPU,
            NULL, NULL, &errNum);
    if (errNum != CL_SUCCESS)
    {
        g2d_printf("Could not create GPU context, trying CPU...\n");
        context = clCreateContextFromType(contextProperties,
                CL_DEVICE_TYPE_CPU,
                NULL, NULL, &errNum);
        if (errNum != CL_SUCCESS)
        {
            g2d_printf("Failed to create an OpenCL GPU or CPU context.\n");
            return NULL;
        }
    }
    return context;
}

static void ReleaseKernel(struct g2dContext *gContext)
{
    if (gContext != NULL) {
        cl_int errNum;

        for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++) {
            if (gContext->kernel[i] != 0)
                errNum = clReleaseKernel(gContext->kernel[i]);
            gContext->kernel[i] = 0;
        }
    }
}

static void ReleaseMemObjects(struct g2dContext *gContext)
{
    if (gContext != NULL) {
        cl_int errNum;

        for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++)
            for(int j = 0; j < MAX_CL_MEM_COUNT; j ++)
                if (gContext->memInObjects[i][j] != NULL) {
                    errNum = clReleaseMemObject(gContext->memInObjects[i][j]);
                    gContext->memInObjects[i][j] = NULL;
                }

        for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++)
            for(int j = 0; j < MAX_CL_MEM_COUNT; j ++)
                if (gContext->memOutObjects[i][j] != NULL) {
                    errNum = clReleaseMemObject(gContext->memOutObjects[i][j]);
                    gContext->memOutObjects[i][j] = NULL;
                }
        //Make sure gpu do release the mem obj
        clFinish(gContext->commandQueue);
    }
}

static void Cleanup(struct g2dContext *gContext)
{
    if (gContext != NULL) {
        cl_int errNum;

        ReleaseMemObjects(gContext);
        //Release gcontext->dst[kernel_index]
        for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++)
            if(gContext->dst[i] != NULL){
                free(gContext->dst[i]);
                gContext->dst[i] == NULL;
            }

        ReleaseKernel(gContext);

        if (gContext->context != 0)
            errNum = clReleaseContext(gContext->context);

        if (gContext->program != 0)
            errNum = clReleaseProgram(gContext->program);

        if (gContext->commandQueue != 0)
            errNum = clReleaseCommandQueue(gContext->commandQueue);

        free(gContext);
    }
}

int cl_g2d_open(void **handle)
{
    int ret;
    struct g2dContext *gContext = NULL;
    cl_device_id device;
    char clFileName[MAX_PATH];
    memset(clFileName, 0, sizeof(clFileName));
    strncpy(clFileName, CL_FILE_PATH, strlen(CL_FILE_PATH));
    //strcat(clFileName, YUYV_CONVERT_SOURCE);
    strcat(clFileName, YUYV_CONVERT_BIN);

    if (handle == NULL) {
        g2d_printf("%s: invalid handle\n", __func__);
        return -1;
    }

    gContext = (struct g2dContext *)calloc(1, sizeof(struct g2dContext));
    if (gContext == NULL) {
        g2d_printf("malloc memory failed for g2dcontext!\n");
        goto err2;
    }

    gContext->context = CreateContext();
	if (gContext->context == NULL) {
		g2d_printf("failed for CreateContext\n");
		goto err2;
    }

    gContext->commandQueue = CreateCommandQueue(gContext->context, &device);
	if (gContext->commandQueue == NULL) {
		g2d_printf("failed for CreateCommandQueue\n");
		goto err2;
    }
    gContext->device = device;
    //All kernel should be built and create in open to save time
    //gContext->program = CreateProgram(gContext->context, device, clFileName);
    gContext->program = CreateProgramFromBinary(gContext->context, device, clFileName);
    if (gContext->program == NULL) {
        g2d_printf("failed for CreateProgramFromBinary %s\n", clFileName);
        goto err2;
    }

    for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++) {
        if(kernel_name_list[i] != NULL) {
            cl_kernel kernel = 0;
            // Create OpenCL kernel
            kernel = clCreateKernel(gContext->program, kernel_name_list[i], NULL);
            if (kernel == NULL)
            {
	            g2d_printf("%s: Cannot create kernel %s\n", __func__, kernel_name_list[i]);
                goto err2;
            }
            gContext->kernel[i] = kernel;
        }
    }

	*handle = (void*)gContext;
	return 0;

err2:
    Cleanup(gContext);
    free(gContext);
    *handle = NULL;
    return -1;
}

int cl_g2d_close(void *handle)
{
    int ret;
    struct g2dContext *gContext = (struct g2dContext *)handle;

    if (gContext == NULL) {
        g2d_printf("%s: invalid handle\n", __func__);
        return -1;
    }

    Cleanup(gContext);
    handle = NULL;

    return 0;
}

int cl_g2d_copy(void *handle, struct cl_g2d_buf *output_buf,
        struct cl_g2d_buf* input_buf, unsigned int size)
{
    cl_int errNum = 0;
    cl_kernel kernel = 0;
    int kernel_index = MEM_COPY_INDEX;
    struct g2dContext *gcontext = (struct g2dContext *)handle;

    if ((gcontext == NULL) ||
       (input_buf == NULL) ||
       (output_buf == NULL)||
       (size > input_buf->buf_size)||
       (size > output_buf->buf_size)) {
        g2d_printf("%s: invalid parameters\n", __func__);
        return -1;
    }

    if((kernel_index >= MAX_CL_KERNEL_COUNT)||(gcontext->dst[kernel_index] != NULL)){
        g2d_printf("%s: invalid kernel index %d\n", __func__, kernel_index);
        return -1;
    }

    kernel = gcontext->kernel[kernel_index];
    cl_mem memObject;
    cl_mem_flags d_flags;

    d_flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;
    if (input_buf->usage == CL_G2D_DEVICE_MEMORY)
        d_flags |= CL_MEM_USE_UNCACHED_HOST_MEMORY_VIV;
    memObject = clCreateBuffer(gcontext->context, d_flags,
        input_buf->buf_size, input_buf->buf_vaddr,
        NULL);
    if (memObject == NULL)
    {
        g2d_printf( "Error creating input memory objects.\n");
        return false;
    }
    gcontext->memInObjects[kernel_index][0] = memObject;


    d_flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    if (output_buf->usage == CL_G2D_DEVICE_MEMORY)
        d_flags |= CL_MEM_USE_UNCACHED_HOST_MEMORY_VIV;
    memObject = clCreateBuffer(gcontext->context, d_flags,
        output_buf->buf_size, output_buf->buf_vaddr,
        NULL);
    if (memObject == NULL)
    {
        g2d_printf( "Error creating output memory objects.\n");
        return false;
    }
    gcontext->memOutObjects[kernel_index][0] = memObject;

    errNum |= clSetKernelArg(kernel, 0,
                    sizeof(cl_mem), &gcontext->memInObjects[kernel_index][0]);
    errNum |= clSetKernelArg(kernel, 1,
                    sizeof(cl_mem), &gcontext->memOutObjects[kernel_index][0]);
    errNum |= clSetKernelArg(kernel, 2,
                    sizeof(cl_mem), &size);
    if (errNum != CL_SUCCESS)
    {
        g2d_printf("Error setting kernel arguments.\n");
        ReleaseMemObjects(gcontext);
        return -1;
    }
    size_t globalWorkSize = size/CL_ATOMIC_COPY_SIZE;
    //Summit command
    errNum = clEnqueueNDRangeKernel(gcontext->commandQueue, kernel, 1, NULL,
            &globalWorkSize, NULL,
            0, NULL, NULL);
    if (errNum != CL_SUCCESS)
    {
        g2d_printf("Error queuing kernel for execution as err = %d \n",
                errNum );
        ReleaseMemObjects(gcontext);
        return -1;
    }
    gcontext->dst[kernel_index] = (struct cl_g2d_surface *)malloc(sizeof(struct cl_g2d_surface));
    if(gcontext->dst[kernel_index] != NULL){
        //Create a fake surface based on YUYV format, w=size/2, h = 1
        gcontext->dst[kernel_index]->planes[0] = (long)output_buf->buf_vaddr;
        gcontext->dst[kernel_index]->format = CL_G2D_YUYV;
        gcontext->dst[kernel_index]->width = size/2;
        gcontext->dst[kernel_index]->height = 1;
        gcontext->dst[kernel_index]->usage = output_buf->usage;
    }
    //use cpu to handle the last 63 bytes, if the size is not n x 64 byptes
    unsigned int remain_size = size&CL_ATOMIC_COPY_MASK;
    if(remain_size > 0)
        memcpy((char *)input_buf->buf_vaddr + globalWorkSize * CL_ATOMIC_COPY_SIZE,
                (char *)output_buf->buf_vaddr + globalWorkSize * CL_ATOMIC_COPY_SIZE, remain_size);

    return 0;
}

int cl_g2d_blit(void *handle, struct cl_g2d_surface *src, struct cl_g2d_surface *dst)
{
    cl_int errNum = 0;
    struct g2dContext *gcontext = (struct g2dContext *)handle;
    cl_kernel kernel = 0;
    int kernel_index = 0;
    cl_int kernel_width = 0;
    cl_int kernel_height = 0;

    if (gcontext == NULL) {
        g2d_printf("%s: invalid handle\n", __func__);
        return -1;
    }

    if ((src->format == CL_G2D_YUYV)&&
        ((dst->format == CL_G2D_NV12) || (dst->format == CL_G2D_YUYV))) {
        if ((src->width < dst->width)&&
            (src->height < dst->height)) {
            g2d_printf("%s: src/dst width/height should be matched\n", __func__);
            return -1;
        }

        if ((src->format == CL_G2D_YUYV)&&
            (dst->format == CL_G2D_NV12))
            kernel_index = YUYV_TO_NV12_INDEX;
        else if ((src->format == CL_G2D_YUYV)&&
            (dst->format == CL_G2D_YUYV))
            kernel_index = YUYV_TO_YUYV_INDEX;
        else
            return -1;

        if((kernel_index >= MAX_CL_KERNEL_COUNT)||(gcontext->dst[kernel_index] != NULL)){
            g2d_printf("%s: invalid kernel index %d\n", __func__, kernel_index);
            return -1;
        }

        kernel = gcontext->kernel[kernel_index];
        //Create memObjects
        if (!CreateMemObjects(gcontext, src, true, kernel_index)){
            g2d_printf("%s: Cannot create input mem objs\n", __func__);
            ReleaseMemObjects(gcontext);
            return -1;
        }
        if (!CreateMemObjects(gcontext, dst, false, kernel_index)){
            g2d_printf("%s: Cannot create output mem objs\n", __func__);
            ReleaseMemObjects(gcontext);
            return -1;
        }
        //Set kernel args
        int arg_index = 0;
        for(int i = 0; i < MAX_CL_MEM_COUNT; i++)
            if (gcontext->memInObjects[kernel_index][i] != NULL){
                errNum |= clSetKernelArg(kernel, arg_index,
                    sizeof(cl_mem), &gcontext->memInObjects[kernel_index][i]);
                arg_index ++;
            }

        for(int i = 0; i < MAX_CL_MEM_COUNT; i ++)
            if (gcontext->memOutObjects[kernel_index][i] != NULL) {
                errNum |= clSetKernelArg(kernel, arg_index, sizeof(cl_mem),
                    &gcontext->memOutObjects[kernel_index][i]);
                arg_index ++;
            }

        if ((src->format == CL_G2D_YUYV)&&
            (dst->format == CL_G2D_NV12)) {
            // for yuyv to nv12, 4 pixels with one kernel calls
            // and based on src width
            int src_width = src->width/4;
            int dst_width = dst->width/4;
            kernel_width = src->width / 4;
            kernel_height = src->height;

            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(src_width));
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(src->height));
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(dst_width));
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(dst->height));
        }
        else if ((src->format == CL_G2D_YUYV)&&
            (dst->format == CL_G2D_YUYV)) {
            // for yuyv to yuyv, 8 pixels with one kernel calls
            // and based on dst width
            int src_width = src->width/8;
            int dst_width = dst->width/8;
            kernel_width = dst->width/8;
            kernel_height = src->height;
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(src_width));
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(src->height));
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(dst_width));
            errNum |= clSetKernelArg(kernel, arg_index++, sizeof(cl_int), &(dst->height));
        }

        if (errNum != CL_SUCCESS)
        {
            g2d_printf("Error setting kernel arguments.\n");
            ReleaseMemObjects(gcontext);
            return -1;
        }
        size_t globalWorkSize[2] = { (size_t)kernel_width, (size_t)kernel_height};
        //Summit command
        errNum = clEnqueueNDRangeKernel(gcontext->commandQueue, kernel, 2, NULL,
                globalWorkSize, NULL,
                0, NULL, NULL);
        if (errNum != CL_SUCCESS)
        {
            g2d_printf("Error queuing kernel for execution as err = %d \n",
                    errNum );
            ReleaseMemObjects(gcontext);
            return -1;
        }
        gcontext->dst[kernel_index] = (struct cl_g2d_surface *)malloc(sizeof(struct cl_g2d_surface));
        if(gcontext->dst[kernel_index] != NULL)
            memcpy(gcontext->dst[kernel_index], dst, sizeof(struct cl_g2d_surface));
    }
    else {
		g2d_printf("%s: cannot support src format 0x%x and dst format 0x%x\n",
                __func__, src->format, dst->format);
        return -1;
    }
    return 0;
}

int cl_g2d_flush(void *handle)
{
	int ret;
	struct g2dContext *gcontext = (struct g2dContext *)handle;

	if (gcontext == NULL) {
		g2d_printf("%s: Invalid handle!\n", __func__);
		return -1;
	}

    clFinish(gcontext->commandQueue);
    ReadOutMemObjects(gcontext);
	return 0;
}

int cl_g2d_finish(void *handle)
{
	int ret;
	struct g2dContext *gcontext = (struct g2dContext *)handle;

	if (gcontext == NULL) {
		g2d_printf("%s: Invalid handle!\n", __func__);
		return -1;
	}
    //Release cl_mem objests
    ReleaseMemObjects(gcontext);
    //Release gcontext->dst[kernel_index]
    for(int i = 0; i < MAX_CL_KERNEL_COUNT; i ++)
        if(gcontext->dst[i] != NULL) {
            free(gcontext->dst[i]);
            gcontext->dst[i] = NULL;
        }

	return 0;
}

