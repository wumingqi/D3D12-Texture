// stdafx.h: 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 项目特定的包含文件
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wincodec.h>
#include <wrl.h>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

// C++ 运行时头文件
#include <string>
#include <array>
#include <vector>

using namespace std;

struct Texture2D
{
	std::vector<UINT8> data;
	UINT width;
	UINT height;
	UINT sizePerPixel;
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT2 uv;
};

struct Utility
{
	static wstring GetModulePath(HMODULE hModule = nullptr)
	{
		wchar_t filename[MAX_PATH];
		GetModuleFileName(hModule, filename, _countof(filename));
		wchar_t *lastSlash = wcsrchr(filename, L'\\');
		if (lastSlash)
		{
			*(lastSlash + 1) = L'\0';
		}
		return { filename };
	}

	static Texture2D LoadPicture(wstring filename)
	{
		static ComPtr<IWICImagingFactory2>	factory;		//WIC工厂
		ComPtr<IWICBitmapDecoder>			decoder;		//解码器
		ComPtr<IWICBitmapFrameDecode>		frame;			//一帧图像数据
		ComPtr<IWICFormatConverter>			format;			//格式转换器
		ComPtr<IWICBitmap>					bmp;			//位图
		ComPtr<IWICBitmapLock>				lock;			//锁，用来访问位图数据

		if (!factory)						//第一次调用函数工厂为空，创建之
		{
			CoCreateInstance(
				CLSID_WICImagingFactory2, nullptr, 
				CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
		}
		
		factory->CreateDecoderFromFilename(	//从文件中创建解码器
			filename.c_str(), 
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnDemand, 
			&decoder);
		
		decoder->GetFrame(0, &frame);		//从解码器中获取一帧图像
		
		factory->CreateFormatConverter(&format);
		auto hr = format->Initialize(
			frame.Get(), 
			GUID_WICPixelFormat32bppRGBA,	//转换图像格式为RGBA格式
			WICBitmapDitherTypeNone, 
			nullptr, 
			1.0, 
			WICBitmapPaletteTypeCustom);
		
		factory->CreateBitmapFromSource(	//利用转换完成的图像数据创建位图
			format.Get(), 
			WICBitmapCacheOnDemand, 
			&bmp);

		/****************************************
		*	以下步骤完成对Texture2D结构体的填写	*
		****************************************/

		Texture2D texture = {};
		texture.sizePerPixel = 4U;							//RGBA格式，每个像素4B
		bmp->GetSize(&texture.width, &texture.height);		//获取位图的宽度和高度（单位：像素）
		WICRect rc = { 0,0,static_cast<INT>(texture.width),static_cast<INT>(texture.height) };
		bmp->Lock(&rc, WICBitmapLockRead, &lock);			//锁定由rc指定区域的图像数据给lock
		
		BYTE *pData = nullptr;			//图像区域首指针
		UINT cbBufferSize = 0;		//图像数据的大小，也即保存数据所需的缓冲区大小
		lock->GetDataPointer(&cbBufferSize, &pData);

		texture.data.resize(cbBufferSize);					//调整缓冲区的大小
		memcpy(&texture.data[0], pData, cbBufferSize);		//复制数据到缓冲区

		return texture;
	}
};
