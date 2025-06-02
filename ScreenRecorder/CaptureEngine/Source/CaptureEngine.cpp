#include "CaptureEngine.h"
#include <iostream>

CaptureEngine::CaptureEngine(int monitor_number, int width, int height)
    : monitor_number_(monitor_number), width_(width), height_(height) 
{
	pixel_format_ = winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized;
	is_application_capturing = false;
	output_callback = nullptr;
	d3d11_device = nullptr;
	d3d_context_ = nullptr;
	frame_pool_ = nullptr;
	session_ = nullptr;
}

bool CaptureEngine::Initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);

    D3D11_CREATE_DEVICE_FLAG create_device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    if (FAILED(D3D11CreateDevice(
        nullptr, 
        D3D_DRIVER_TYPE_HARDWARE, 
        nullptr,
        create_device_flags, nullptr, 0,
        D3D11_SDK_VERSION, d3d11_device.put(), nullptr,
        d3d_context_.put()))) 
    {
        return false;
    }

    capture_item_ = GetMonitorCaptureItemFromIndex(monitor_number_);
    
    winrt::com_ptr<IDXGIDevice> dxgi_device;
    d3d11_device.as(dxgi_device);

    winrt::com_ptr<::IInspectable> inspectable_device;
    HRESULT hr = CreateDirect3D11DeviceFromDXGIDevice(dxgi_device.get(), inspectable_device.put());
    if (FAILED(hr))
    {
        return false;
    }

    d3d_device_ = inspectable_device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

	width_ = capture_item_.Size().Width;
	height_ = capture_item_.Size().Height;
  
    return true;
}

void CaptureEngine::Reinitialize()
{
	if (frame_pool_)
	{
        frame_pool_.Recreate(
            d3d_device_,
            pixel_format_,
            2,
            { width_, height_ });
	}
}


void CaptureEngine::StartCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!capture_item_) return;

	frame_pool_ = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
		d3d_device_,
        pixel_format_,
		2,
        {width_, height_});

    frame_pool_.FrameArrived({ this, &CaptureEngine::OnFrameArrived });
    
    session_ = frame_pool_.CreateCaptureSession(capture_item_);
    session_.StartCapture();
}

void CaptureEngine::StopCapture()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (session_) 
    {
        session_.Close();
    }

    if (frame_pool_)
    {
        frame_pool_.Close();
    }
}

void CaptureEngine::SetOutputCallback(OutputBufferCallback output_callback)
{
    this->output_callback = output_callback;
}

bool CaptureEngine::CaptureWindow(HWND window_handle)
{
    capture_item_ = GetWindowCaptureItem(window_handle);
    return capture_item_ != nullptr;
}

bool CaptureEngine::CaptureMonitor(HMONITOR monitor)
{
    capture_item_ = GetMonitorCaptureItem(monitor);

	if (!capture_item_)
	{
		width_ = capture_item_.Size().Width;
		height_ = capture_item_.Size().Height;
	}

    return capture_item_ != nullptr;
}

int CaptureEngine::GetCaptureItemWidth()
{
    if (capture_item_)
    {
        return capture_item_.Size().Width;
    }

    return 0;
}

int CaptureEngine::GetCaptureItemHeight()
{
    if (capture_item_)
    {
        return capture_item_.Size().Height;
    }

    return 0;
}

CaptureEngine::~CaptureEngine()
{
	StopCapture();
}

void CaptureEngine::OnFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool sender, winrt::Windows::Foundation::IInspectable const& args)
{
    auto frame = sender.TryGetNextFrame();
    auto surface = frame.Surface();

    int input_width = static_cast<int>(frame.ContentSize().Width);
    int input_height = static_cast<int>(frame.ContentSize().Height);

    if (!is_application_capturing && (input_width != width_ || input_height != height_)) //For Resolution changes 
    {
		width_ = input_width;
		height_ = input_height;

		Reinitialize();
		return;
    }

    int output_width = 0;
    int output_height = 0;
	std::vector<uint8_t> image_buffer;

    if (output_callback
		&& ConvertSurfaceToImageBuffer(surface, input_width, input_height, image_buffer, output_width, output_height)
        && output_width > 0 && output_height > 0)
    {
        output_callback(image_buffer, output_width, output_height);
    }
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureEngine::GetMonitorCaptureItemFromIndex(int monitor_index)
{
    ComPtr<IDXGIFactory1> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    ComPtr<IDXGIAdapter1> adapter;
    factory->EnumAdapters1(0, &adapter);

    ComPtr<IDXGIOutput> output;
    int index = 0;
    while (adapter->EnumOutputs(index, &output) != DXGI_ERROR_NOT_FOUND) 
    {
        if (index == (monitor_index - 1)) {
            DXGI_OUTPUT_DESC desc;
            output->GetDesc(&desc);
            return GetMonitorCaptureItem(desc.Monitor);
        }
        output.Reset();
        index++;
    }
    return nullptr;
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureEngine::GetWindowCaptureItem(HWND window_handle)
{
    auto interop_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };

    interop_factory->CreateForWindow(window_handle, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), winrt::put_abi(item));

    is_application_capturing = item != nullptr;

    return item;
}


winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureEngine::GetMonitorCaptureItem(HMONITOR monitor)
{
    auto interop_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };

    interop_factory->CreateForMonitor(monitor, winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(), winrt::put_abi(item));
    return item;
}

bool CaptureEngine::ConvertSurfaceToImageBuffer(winrt::IDirect3DSurface const& surface, int input_width, int input_height, 
                                                std::vector<uint8_t>& image_buffer, int& output_width, int& output_height)
{

	std::lock_guard<std::mutex> lock(mutex_);

	if (!surface) return false;
    bool result = false;

    ComPtr<ID3D11Texture2D> current_frame_texture = GetTextureFromSurface(surface);

    if (d3d11_device && d3d_context_.get())
    {

        D3D11_TEXTURE2D_DESC desc{};
        current_frame_texture->GetDesc(&desc);


        D3D11_TEXTURE2D_DESC staging_desc = desc;
        staging_desc.Usage = D3D11_USAGE_STAGING;
        staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        staging_desc.BindFlags = 0;
        staging_desc.MiscFlags = 0;

		if (is_application_capturing)
		{
			staging_desc.Width = width_;
			staging_desc.Height = height_;
		}

        size_t buffer_size = staging_desc.Width * staging_desc.Height * 4;

        ComPtr<ID3D11Texture2D> staging_texture;
        HRESULT hr = d3d11_device->CreateTexture2D(&staging_desc, nullptr, staging_texture.GetAddressOf());

        if (!is_application_capturing)
        {
            d3d_context_->CopyResource(staging_texture.Get(), current_frame_texture.Get());
        }
        else
        {

            D3D11_BOX srcBox = {};

            srcBox.left = 0;
            srcBox.top = 0;

            srcBox.right = input_width;
            srcBox.bottom = input_height;

            srcBox.front = 0;
            srcBox.back = 1;

            int offset_x = 0; 
            int offset_y = 0;

            if (width_ > input_width)
            {
                offset_x = (width_ - input_width) / 2;
            }

			if (height_ > input_height)
			{
				offset_y = (height_ - input_height) / 2;
			}

            d3d_context_->CopySubresourceRegion(
                staging_texture.Get(),       // Destination texture
                0,                           // Subresource index
                offset_x, offset_y, 0,       // Destination X, Y, Z
                current_frame_texture.Get(), // Source texture
                0,                           // Source subresource
                &srcBox                      // Source box
            );

            buffer_size = width_ * height_ * 4;
        }

        image_buffer.reserve(buffer_size);

        D3D11_MAPPED_SUBRESOURCE mapped_resource{};
        hr = d3d_context_->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped_resource);

        if (SUCCEEDED(hr))
        {
            uint8_t* src = static_cast<uint8_t*>(mapped_resource.pData);
            image_buffer.insert(image_buffer.end(), src, src + buffer_size);

            output_width = staging_desc.Width;
            output_height = staging_desc.Height;

			result = true;
        }

        d3d_context_->Unmap(staging_texture.Get(), 0);
    }
    else
    {
        throw std::runtime_error("Failed to create staging texture.");
    }

    return result;
}

ComPtr<ID3D11Texture2D> CaptureEngine::GetTextureFromSurface(winrt::IDirect3DSurface const& surface)
{  
    winrt::com_ptr<IInspectable> inspectable = surface.as<IInspectable>();
    ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> dxgiInterfaceAccess;

    HRESULT hr = inspectable->QueryInterface(IID_PPV_ARGS(&dxgiInterfaceAccess));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to query IDirect3DDxgiInterfaceAccess.");
    }

   ComPtr<ID3D11Texture2D> texture;
    hr = dxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&texture));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to get ID3D11Texture2D.");
    }

    return texture;
}



