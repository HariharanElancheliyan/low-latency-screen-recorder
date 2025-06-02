#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h> 
#include <dxgi1_4.h> 
#include <memory>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <wrl/client.h>
#include <windows.graphics.directx.direct3d11.h>
#include <windows.media.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <functional>
#include <winrt/impl/windows.graphics.capture.0.h>
#include <Windows.Graphics.Capture.Interop.h>
#include <mutex>

namespace winrt
{
    using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
    using namespace winrt::Windows::Graphics::Capture;
    using namespace winrt::Windows::Graphics::DirectX;

}

using namespace Microsoft::WRL;

using OutputBufferCallback = std::function<void(std::vector<uint8_t>&, int, int)>;

class CaptureEngine 
{

public:
    CaptureEngine(int monitor_number, int width, int height);

    bool Initialize();
    void Reinitialize();
    void StartCapture();
    void StopCapture();
    void SetOutputCallback(OutputBufferCallback output_callback);

    bool CaptureWindow(HWND window_handle);
	bool CaptureMonitor(HMONITOR monitor);

    int  GetCaptureItemWidth();
    int  GetCaptureItemHeight();

    ~ CaptureEngine();

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool sender,
        winrt::Windows::Foundation::IInspectable const& args);

    winrt::GraphicsCaptureItem GetMonitorCaptureItemFromIndex(int monitor_index);
    winrt::GraphicsCaptureItem GetWindowCaptureItem(HWND window_handle);
    winrt::GraphicsCaptureItem GetMonitorCaptureItem(HMONITOR monitor);

    bool ConvertSurfaceToImageBuffer(winrt::IDirect3DSurface const& surface, int input_width, int input_height, 
                                     std::vector<uint8_t>& image_buffer, int& output_width, int& output_height);


    ComPtr<ID3D11Texture2D> GetTextureFromSurface(winrt::IDirect3DSurface const& surface);


    OutputBufferCallback output_callback;

    int width_;
    int height_;
    int monitor_number_;

    bool is_application_capturing;


    winrt::GraphicsCaptureItem capture_item_{ nullptr };
    winrt::Direct3D11CaptureFramePool frame_pool_{ nullptr };
    winrt::GraphicsCaptureSession session_{ nullptr };
	winrt::DirectXPixelFormat pixel_format_ = winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized;
    winrt::event_token frame_arrived_token;

    winrt::IDirect3DDevice d3d_device_;
	winrt::com_ptr<ID3D11Device> d3d11_device;
    winrt::com_ptr<ID3D11DeviceContext> d3d_context_;
    winrt::com_ptr<IDXGISwapChain3> swap_chain;

    ComPtr<ID3D11Texture2D> current_frame;
	std::mutex mutex_;

};