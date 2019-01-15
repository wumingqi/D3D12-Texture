#include "stdafx.h"

HRESULT hr = 0;

class Application
{
	static const UINT FrameCount = 2U;
	UINT m_frameIndex;
	ComPtr<ID3D12Resource>				m_renderTargets[FrameCount];
	ComPtr<ID3D12DescriptorHeap>		m_rtvHeap;
	UINT								m_rtvDescriptorSize;

	//纹理资源
	Texture2D							m_img;
	ComPtr<ID3D12DescriptorHeap>		m_srvHeap;
	ComPtr<ID3D12Resource>				m_texture;

	ComPtr<ID3D12Resource>				m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_vertexBufferView;

	ComPtr<ID3D12Device>				m_device;
	ComPtr<ID3D12CommandQueue>			m_commandQueue;
	ComPtr<ID3D12CommandAllocator>		m_commandAllocator[FrameCount];
	ComPtr<ID3D12GraphicsCommandList>			m_commandList;
	ComPtr<ID3D12RootSignature>			m_rootSignature;
	ComPtr<ID3D12PipelineState>			m_pipelineState;

	ComPtr<IDXGIFactory6>				m_dxgiFactory;
	ComPtr<IDXGISwapChain4>				m_swapChain;

	ComPtr<ID3D12Fence>					m_fence;
	UINT64								m_fenceValues[FrameCount];
	HANDLE m_fenceEvent;

private:
	void OnInit()
	{
		m_img = Utility::LoadPicture(L"github.jpg");
		LoadPipeline();
		LoadAssets();
	}

	void LoadPipeline()
	{
		UINT dxgiFlags = 0;
#if defined(_DEBUG)
		ComPtr<ID3D12Debug> debugController;
		D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();

		dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));

		CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&m_dxgiFactory));

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_width;
		swapChainDesc.Height = m_height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;

		ComPtr<IDXGISwapChain1> swapChain;
		m_dxgiFactory->CreateSwapChainForHwnd(
			m_commandQueue.Get(), m_hWnd, 
			&swapChainDesc, nullptr, nullptr, &swapChain);
		swapChain.As(&m_swapChain);
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

		m_dxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

		//创建描述符堆
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};			//RTV
			rtvHeapDesc.NumDescriptors = FrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

			D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
			srvHeapDesc.NumDescriptors = 1;
			srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap));

			m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		//创建帧资源
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < FrameCount; i++)
			{
				m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
				m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
				rtvHandle.ptr += m_rtvDescriptorSize;

				m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i]));
			}
		}
	}

	void LoadAssets()
	{
		//Root Signature
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

			if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			D3D12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };

			D3D12_ROOT_PARAMETER1 rootParameters[1];
			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[0].DescriptorTable.pDescriptorRanges = ranges;

			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 0;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = { D3D_ROOT_SIGNATURE_VERSION_1_1 };
			rootSignatureDesc.Desc_1_1 = { _countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };

			ComPtr<ID3DBlob> signature, error;
			D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);
			m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
		}

		//PSO
		{
			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			wstring filename = Utility::GetModulePath().append(L"shaders.hlsl");
			D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
			D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);

			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// Describe and create the graphics pipeline state object (PSO).
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			psoDesc.pRootSignature = m_rootSignature.Get();
			psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
			psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;
			m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
		}

		m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList));

		{
			float x = m_img.width / (float)m_width;
			float y = m_img.height / (float)m_height;
			m_aspectRatio = 1.f;
			Vertex triangleVertices[] =
			{
				{ { -x, -y * m_aspectRatio, 0.0f },		{ 0.f, 1.0f } },
				{ { -x, y * m_aspectRatio, 0.0f },		{ 0.f, 0.0f } },
				{ { x, -y * m_aspectRatio, 0.0f },		{ 1.f, 1.0f } },
				{ { x, y * m_aspectRatio, 0.0f },		{ 1.f, 0.0f } },
			};

			const UINT vertexBufferSize = sizeof(triangleVertices);

			m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_vertexBuffer));

			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			m_vertexBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
			m_vertexBufferView.StrideInBytes = sizeof(Vertex);
			m_vertexBufferView.SizeInBytes = vertexBufferSize;
		}

		ComPtr<ID3D12Resource> textureUploadHeap;

		{
			// Describe and create a Texture2D.
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.MipLevels = 1;
			textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			textureDesc.Width = m_img.width;
			textureDesc.Height = m_img.height;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

			m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&m_texture));

			const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
			m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&textureUploadHeap));

			std::vector<UINT8> texture = m_img.data;

			D3D12_SUBRESOURCE_DATA textureData = {};
			textureData.pData = &texture[0];
			textureData.RowPitch = m_img.width * m_img.sizePerPixel;
			textureData.SlicePitch = textureData.RowPitch * m_img.height;

			UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
			
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;
			m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		m_commandList->Close();
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		
		{
			m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
			m_fenceValues[m_frameIndex]++;
			m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			WaitForGpu();
		}
	}

	void OnUpdate()
	{

	}
	void OnRender()
	{
		PopulateCommandList();
		ID3D12CommandList* ppCmdLists[] = { m_commandList.Get() };
		m_commandQueue->ExecuteCommandLists(_countof(ppCmdLists), ppCmdLists);
		m_swapChain->Present(1, 0);

		MoveToNextFrame();
	}

	void PopulateCommandList()
	{
		m_commandAllocator[m_frameIndex]->Reset();
		m_commandList->Reset(m_commandAllocator[m_frameIndex].Get(), m_pipelineState.Get());

		m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
		m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		m_commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		m_commandList->RSSetViewports(1, &m_viewport);
		m_commandList->RSSetScissorRects(1, &m_scissorRect);

		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
		m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		const float clearColor[] = { 0.2f, 0.4f, 0.8f, 1.0f };
		m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		m_commandList->DrawInstanced(4, 1, 0, 0);

		// Indicate that the back buffer will now be used to present.
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		m_commandList->Close();
	}

	void OnDestroy()
	{
		WaitForGpu();
		CloseHandle(m_fenceEvent);
	}
	void WaitForGpu()
	{
		m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]);
		m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
		m_fenceValues[m_frameIndex]++;
	}

	void MoveToNextFrame()
	{
		const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
		m_commandQueue->Signal(m_fence.Get(), currentFenceValue);
		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
		if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
		{
			m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
			WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
		}
		m_fenceValues[m_frameIndex] = currentFenceValue + 1;
	}

private:
	HINSTANCE m_hInstance;
	HWND m_hWnd;
	UINT m_width, m_height;
	float m_aspectRatio;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		auto app = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		switch (msg)
		{
		case WM_PAINT:
			app->OnUpdate();
			app->OnRender();
			ValidateRect(hWnd, nullptr);
			break;
		case WM_DESTROY:
			app->OnDestroy();
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
		return 0;
	}
public:
	Application(UINT width, UINT height, HINSTANCE hInstance = nullptr):
		m_hInstance(hInstance),
		m_width(width),
		m_height(height),
		m_aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
		m_fenceValues{},
		m_viewport{ 0.f,0.f,static_cast<float>(width) , static_cast<float>(height), 0.f,1.f },
		m_scissorRect{ 0,0,static_cast<LONG>(width) , static_cast<LONG>(height) }
	{}

	int Run(int nCmdShow)
	{
		WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
		wc.style = CS_VREDRAW | CS_HREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = m_hInstance;
		wc.hIcon = LoadIcon(m_hInstance, IDI_APPLICATION);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
		wc.lpszMenuName = nullptr;
		wc.lpszClassName = L"HelloDirect3D";
		wc.hIconSm = nullptr;

		RegisterClassEx(&wc);

		DWORD style = WS_OVERLAPPEDWINDOW; DWORD styleEx = 0;
		RECT rc = { 0,0,(LONG)m_width,(LONG)m_height };
		AdjustWindowRectEx(&rc, style, false, styleEx);
		auto cx = GetSystemMetrics(SM_CXSCREEN);
		auto cy = GetSystemMetrics(SM_CYFULLSCREEN);
		auto w = rc.right - rc.left;
		auto h = rc.bottom - rc.top;
		auto x = (cx - w) / 2;
		auto y = (cy - h) / 2;

		m_hWnd = CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, 
			L"Hello Texture", WS_OVERLAPPEDWINDOW,
			x, y, w, h, nullptr, nullptr, m_hInstance, nullptr);

		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		OnInit();

		ShowWindow(m_hWnd, nCmdShow);
		MSG msg = {};
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return static_cast<int>(msg.wParam);
	}
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, INT nCmdShow)
{
	CoInitialize(nullptr);
	return Application(1280, 720, hInstance).Run(nCmdShow);
}