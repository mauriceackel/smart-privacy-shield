extern "C"
{
#include "screencaputil.h"
#include "gstscreencapsrc.h"
#include "winanalyzerutils.h"

#include <windowmeta.h>
#include <windowlocationsmeta.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/video/video.h>
}

#include <Windows.h>
#include <Unknwn.h>
#include <inspectable.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3d11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.capture.h>
#include <profileapi.h>

#include <d3d11.h>
#include <d2d1.h>
#include <d3dcommon.h>

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
}

G_DEFINE_TYPE(GstScreenCapUtil, gst_screen_cap_util, G_TYPE_OBJECT);

struct _FrameData
{
    gint width, height;
    gint x = 0, y = 0;
    size_t stride, size;
    GArray *windowInfos;
    guint8 *data;
};
typedef struct _FrameData FrameData;

struct _GstScreenCapUtilPrivate
{
    winrt::Direct3D11CaptureFramePool framePool;
    winrt::GraphicsCaptureSession captureSession;
    winrt::IDirect3DDevice d3dDevice;
    winrt::com_ptr<ID3D11Device> dxgiDevice;
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;

    // Staging texture that can be read by the CPU
    D3D11_TEXTURE2D_DESC desc;
    winrt::com_ptr<ID3D11Texture2D> stagingTexture;

    // Synchronized frame exchange
    FrameData *frameData;
    GMutex mtxFrameData;
    GCond condFrameData;
};

// -----------------------------------------------------------------------------------------------------
// -------------------------------------------- Cpp/WinRT ----------------------------------------------
// -----------------------------------------------------------------------------------------------------

static auto create_capture_item_from_handle(guintptr handle, SpsSourceType type)
{
    auto interopFactory = winrt::get_activation_factory<winrt::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::GraphicsCaptureItem item = {nullptr};

    if (type == SOURCE_TYPE_WINDOW)
    {
        winrt::check_hresult(interopFactory->CreateForWindow((HWND)handle, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item)));
    }
    else if (type == SOURCE_TYPE_DISPLAY)
    {
        winrt::check_hresult(interopFactory->CreateForMonitor((HMONITOR)handle, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item)));
    }

    return item;
}

struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetInterface(GUID const &id, void **object) = 0;
};

template <typename T>
static auto get_dxgi_interface_from_object(winrt::IInspectable const &object)
{
    auto access = object.as<IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<T> result;
    winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
    return result;
}

static auto create_direct_3d_device_with_driver(D3D_DRIVER_TYPE const type, winrt::com_ptr<ID3D11Device> &device)
{
    WINRT_ASSERT(!device);

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // Call actual system API
    return D3D11CreateDevice(nullptr, type, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, device.put(), nullptr, nullptr);
}

extern "C"
{
    HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(::IDXGIDevice *dxgiDevice, ::IInspectable **graphicsDevice);
}

static auto create_direct_3d_device()
{
    // Create base device
    winrt::com_ptr<ID3D11Device> baseDevice;
    HRESULT hr = create_direct_3d_device_with_driver(D3D_DRIVER_TYPE_HARDWARE, baseDevice);
    if (DXGI_ERROR_UNSUPPORTED == hr)
        hr = create_direct_3d_device_with_driver(D3D_DRIVER_TYPE_WARP, baseDevice);
    winrt::check_hresult(hr);

    // Convert to dxgi
    winrt::com_ptr<IDXGIDevice> dxgiDevice = baseDevice.as<IDXGIDevice>();

    // Convert dxgi device to WinRT Direct3D
    winrt::com_ptr<::IInspectable> d3dDevice;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), d3dDevice.put()));

    return d3dDevice.as<winrt::IDirect3DDevice>();
}

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Private Helper --------------------------------------------
// -----------------------------------------------------------------------------------------------------

void get_frame_size(winrt::Direct3D11CaptureFrame *imageRef, gint *width, gint *height, size_t *stride, size_t *size)
{
    auto const contentSize = (*imageRef).ContentSize();

    if (width)
        *width = contentSize.Width;

    if (height)
        *height = contentSize.Height;

    size_t str = contentSize.Width * BYTES_PER_PIXEL;
    if (stride)
        *stride = str;

    size_t s = str * contentSize.Height;
    if (size)
        *size = s;
}

void frame_arrived_handler(GstScreenCapUtil *self, GstScreenCapSrc *src)
{
    // If requested, add position of windows
    GArray *windowInfos = NULL;
    // TODO: Check if initialized needed
    if (src->initialized && src->sourceType == SOURCE_TYPE_DISPLAY && src->reportWindowLocations)
    {
        windowInfos = g_array_new(FALSE, FALSE, sizeof(WindowInfo));
        g_array_set_clear_func(windowInfos, window_info_clear);
        get_visible_windows(src->sourceId, windowInfos);
    }

    // Get window coordinates
    RECT boundingRect;
    gboolean addWindowCoordinates = FALSE;
    if (src->sourceType == SOURCE_TYPE_WINDOW)
    {
        addWindowCoordinates = GetWindowRect((HWND)src->sourceId, &boundingRect);
    }

    // Get frame
    winrt::Direct3D11CaptureFrame imageRef = self->priv->framePool.TryGetNextFrame();

    // Lock mutex to be able to change frameData
    g_mutex_lock(&self->priv->mtxFrameData);

    // Set default values for each frame
    if (addWindowCoordinates)
    {
        self->priv->frameData->x = boundingRect.left;
        self->priv->frameData->y = boundingRect.top;
    }
    else
    {
        self->priv->frameData->x = 0;
        self->priv->frameData->y = 0;
    }

    // Get frame size
    get_frame_size(&imageRef, &self->priv->frameData->width, &self->priv->frameData->height, &self->priv->frameData->stride, &self->priv->frameData->size);

    // Extract surface
    auto surfaceTexture = get_dxgi_interface_from_object<ID3D11Texture2D>(imageRef.Surface());

    // Check if we need to update staging texture for extraction
    if (self->priv->stagingTexture)
        self->priv->stagingTexture->GetDesc(&self->priv->desc);

    if (!self->priv->stagingTexture || self->priv->desc.Width != self->priv->frameData->width || self->priv->desc.Height != self->priv->frameData->height)
    {
        // Adapt staging texture
        surfaceTexture->GetDesc(&self->priv->desc);
        self->priv->desc.Usage = D3D11_USAGE_STAGING;
        self->priv->desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        self->priv->desc.BindFlags = 0;
        self->priv->desc.MiscFlags = 0;

        winrt::check_hresult(self->priv->dxgiDevice->CreateTexture2D(&self->priv->desc, nullptr, self->priv->stagingTexture.put()));
    }

    // Copy frame surface to staging surtface
    self->priv->d3dContext->CopyResource(self->priv->stagingTexture.get(), surfaceTexture.get());
    imageRef.Close();

    // Map the staging resource for CPU access
    D3D11_MAPPED_SUBRESOURCE mapInfo;
    winrt::check_hresult(self->priv->d3dContext->Map(self->priv->stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapInfo));
    auto source = reinterpret_cast<byte *>(mapInfo.pData);
    self->priv->frameData->data = (guint8 *)g_malloc(self->priv->frameData->width * self->priv->frameData->height * BYTES_PER_PIXEL);
    for (auto i = 0; i < (int)self->priv->frameData->height; i++)
    {
        memcpy((self->priv->frameData->data + i * self->priv->frameData->width * BYTES_PER_PIXEL), source, self->priv->frameData->width * BYTES_PER_PIXEL);
        source += mapInfo.RowPitch;
    }
    self->priv->d3dContext->Unmap(self->priv->stagingTexture.get(), 0);

    // Add windowInfo
    if (self->priv->frameData->windowInfos)
        g_array_free(self->priv->frameData->windowInfos, TRUE);
    self->priv->frameData->windowInfos = windowInfos;

    // Signal frame processed
    g_cond_signal(&self->priv->condFrameData);

    g_mutex_unlock(&self->priv->mtxFrameData);
}

// -----------------------------------------------------------------------------------------------------
// ----------------------------------------- Object Methods --------------------------------------------
// -----------------------------------------------------------------------------------------------------

gboolean gst_screen_cap_util_get_dimensions(GstScreenCapUtil *self, GstScreenCapSrc *src, gint *width, gint *height, size_t *size)
{
    if (G_UNLIKELY(!self->initialized))
        return FALSE;

    g_mutex_lock(&self->priv->mtxFrameData);

    if (width)
        *width = self->priv->frameData->width;
    if (height)
        *height = self->priv->frameData->height;
    if (size)
        *size = self->priv->frameData->size;

    g_mutex_unlock(&self->priv->mtxFrameData);

    return TRUE;
}

GstFlowReturn gst_screen_cap_util_create_buffer(GstScreenCapUtil *self, GstScreenCapSrc *src, GstBuffer **buffer)
{
    if (G_UNLIKELY(!self->initialized))
        return GST_FLOW_ERROR;

    GST_DEBUG_OBJECT(src, "Creating buffer");

    GstFlowReturn ret = GST_FLOW_OK;

    if (G_UNLIKELY(GST_VIDEO_INFO_FORMAT(&src->info) == GST_VIDEO_FORMAT_UNKNOWN))
        return GST_FLOW_NOT_NEGOTIATED;

    GstBufferPool *pool;
    GstVideoFrame frame;
    GstWindowMeta *windowMeta;
    GstWindowLocationsMeta *windowLocationsMeta;

    g_mutex_lock(&self->priv->mtxFrameData);

    if (src->info.width != self->priv->frameData->width || src->info.height != self->priv->frameData->height)
    {
        // Set new caps according to current frame dimensions
        GST_WARNING("Output frame size has changed %dx%d -> %dx%d, updating caps", src->info.width, src->info.height, self->priv->frameData->width, self->priv->frameData->height);

        GstCaps *newCaps = gst_caps_copy(src->caps);
        gst_caps_set_simple(newCaps, "width", G_TYPE_INT, self->priv->frameData->width, "height", G_TYPE_INT, self->priv->frameData->height, NULL);
        gst_base_src_set_caps(GST_BASE_SRC(src), newCaps);
        gst_base_src_negotiate(GST_BASE_SRC(src));
    }

    // Get buffer from bufferpool
    pool = gst_base_src_get_buffer_pool(GST_BASE_SRC(src));
    ret = gst_buffer_pool_acquire_buffer(pool, buffer, NULL);
    if (G_UNLIKELY(ret != GST_FLOW_OK))
    {
        gst_object_unref(pool);
        return ret;
    }

    // Add metadata
    windowMeta = GST_WINDOW_META_ADD(*buffer);
    windowMeta->width = self->priv->frameData->width;
    windowMeta->height = self->priv->frameData->height;
    windowMeta->x = self->priv->frameData->x;
    windowMeta->y = self->priv->frameData->y;

    if (self->priv->frameData->windowInfos)
    {
        windowLocationsMeta = GST_WINDOW_LOCATIONS_META_ADD(*buffer);
        WindowInfo newInfo;
        for(gint i = 0; i < self->priv->frameData->windowInfos->len; i++)
        {
            window_info_copy(&g_array_index(self->priv->frameData->windowInfos, WindowInfo, i), &newInfo);
            g_array_append_val(windowLocationsMeta->windowInfos, newInfo);
        }
    }

    // Get videoframe from buffer
    if (!gst_video_frame_map(&frame, &src->info, *buffer, GST_MAP_WRITE))
    {
        gst_object_unref(pool);
        return GST_FLOW_ERROR;
    }

    // Check if buffer has correct size for captured image
    if (frame.info.size != self->priv->frameData->size)
    {
        GST_DEBUG_OBJECT(src, "Frame has wrong size (%zu instead of %zu)", frame.info.size, self->priv->frameData->size);
        gst_video_frame_unmap(&frame);
        gst_object_unref(pool);
        return GST_FLOW_ERROR;
    }

    GST_DEBUG_OBJECT(src, "Captured frame. Width: %i, Height: %i, Stride: %zu, Size: %zu for buffer of size: %zu!", self->priv->frameData->width, self->priv->frameData->height, self->priv->frameData->stride, self->priv->frameData->size, frame.info.size);

    guint8 *data = (guint8 *)GST_VIDEO_FRAME_PLANE_DATA(&frame, 0);
    memcpy(data, self->priv->frameData->data, self->priv->frameData->size);

    // Free memory
    gst_video_frame_unmap(&frame);
    gst_object_unref(pool);

    GST_DEBUG_OBJECT(src, "Done creating buffer");

    g_mutex_unlock(&self->priv->mtxFrameData);

    return ret;
}

void gst_screen_cap_util_initialize(GstScreenCapUtil *self, GstScreenCapSrc *src)
{
    if (self->initialized)
        return;

    // Prepare thread management
    g_mutex_init(&self->priv->mtxFrameData);
    g_cond_init(&self->priv->condFrameData);

    // Get a Direct3D device for capturing
    self->priv->d3dDevice = create_direct_3d_device();
    self->priv->dxgiDevice = get_dxgi_interface_from_object<ID3D11Device>(self->priv->d3dDevice);

    // Extract context from DXGI device
    self->priv->dxgiDevice->GetImmediateContext(self->priv->d3dContext.put());

    // Get a capture item from window/monitor handle
    auto captureItem = create_capture_item_from_handle(src->sourceId, src->sourceType);

    // Create frame pool
    self->priv->framePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(self->priv->d3dDevice, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 1, captureItem.Size());
    self->priv->captureSession = self->priv->framePool.CreateCaptureSession(captureItem);
    self->priv->frameData = (FrameData *)g_malloc(sizeof(FrameData));
    self->priv->frameData->windowInfos = NULL;
    self->priv->frameData->data = NULL;

    // Register frame handler
    auto handlerLambda = [self, src](winrt::Direct3D11CaptureFramePool const &sender, winrt::IInspectable const &foo)
    { frame_arrived_handler(self, src); };
    self->priv->framePool.FrameArrived(handlerLambda);

    // Start capturing
    self->priv->captureSession.StartCapture();

    // Capture first frame
    g_mutex_lock(&self->priv->mtxFrameData);
    while (self->priv->frameData->data == NULL)
        g_cond_wait(&self->priv->condFrameData, &self->priv->mtxFrameData);
    g_mutex_unlock(&self->priv->mtxFrameData);

    self->initialized = TRUE;
}

void gst_screen_cap_util_finalize(GstScreenCapUtil *self)
{
    if (!self->initialized)
        return;

    // Stop capturing & close frame pool
    self->priv->captureSession.Close();
    self->priv->framePool.Close();

    // Clear thread syncing
    g_mutex_clear(&self->priv->mtxFrameData);
    g_cond_clear(&self->priv->condFrameData);

    // Deinit variables
    self->priv->captureSession = NULL;
    self->priv->framePool = NULL;

    self->priv->d3dDevice.Close();
    self->priv->d3dDevice = NULL;

    self->priv->d3dContext = NULL;
    self->priv->dxgiDevice = NULL;

    self->priv->stagingTexture = NULL;
    if (self->priv->frameData)
    {
        if (self->priv->frameData->data)
            g_free((void *)self->priv->frameData->data);

        g_free((void *)self->priv->frameData);
    }

    self->initialized = FALSE;
}

// Object instantiation
static void gst_screen_cap_util_init(GstScreenCapUtil *self)
{
    // Init private data
    self->priv = g_new0(GstScreenCapUtilPrivate, 1);

    self->priv->dxgiDevice = NULL;
    self->priv->d3dContext = NULL;
    self->priv->stagingTexture = NULL;

    // Set default values
    self->initialized = FALSE;
}

// Object finalization
static void gst_screen_cap_util_object_finalize(GObject *object)
{
    GstScreenCapUtil *self = GST_SCREEN_CAP_UTIL(object);

    // Default finalize routine
    gst_screen_cap_util_finalize(self);
    // Free private data

    g_free((void *)self->priv);

    G_OBJECT_CLASS(gst_screen_cap_util_parent_class)->finalize(object);
}

static void gst_screen_cap_util_class_init(GstScreenCapUtilClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = gst_screen_cap_util_object_finalize;
}

GstScreenCapUtil *gst_screen_cap_util_new()
{
    return (GstScreenCapUtil *)g_object_new(GST_TYPE_SCREEN_CAP_UTIL, NULL);
}
