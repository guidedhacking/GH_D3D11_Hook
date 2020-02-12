#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define safe_release(p) if (p) { p->Release(); p = nullptr; } 

#include "shadez.h"
#include "D3D_VMT_Indices.h"
#define VMT_PRESENT (UINT)IDXGISwapChainVMT::Present
#define PRESENT_STUB_SIZE 5

// d3d11 related object ptrs
using namespace DirectX;

ID3D11Device* pDevice = nullptr;
IDXGISwapChain* pSwapchain = nullptr;
ID3D11DeviceContext* pContext = nullptr;
ID3D11RenderTargetView* pRenderTargetView = nullptr;
ID3D11VertexShader* pVertexShader = nullptr;
ID3D11InputLayout* pVertexLayout = nullptr;
ID3D11PixelShader* pPixelShader = nullptr;
ID3D11Buffer* pVertexBuffer = nullptr;
ID3D11Buffer* pIndexBuffer = nullptr;
ID3D11Buffer* pConstantBuffer = nullptr;

// Changing this to an array of viewports
#define MAINVP 0
D3D11_VIEWPORT pViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{ 0 };
XMMATRIX mOrtho;

struct ConstantBuffer
{
	XMMATRIX mProjection;
};

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

HRESULT __stdcall hkPresent( IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags );
using fnPresent = HRESULT( __stdcall* )(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);
void* ogPresent;					// Pointer to the original Present function
fnPresent ogPresentTramp;			// Function pointer that calls the Present stub in our trampoline
void* pTrampoline = nullptr;		// Pointer to jmp instruction in our trampoline that leads to hkPresent
char ogBytes[PRESENT_STUB_SIZE];	// Buffer to store original bytes from Present

bool Hook( void* pSrc, void* pDst, size_t size );
bool WriteMem( void* pDst, char* pBytes, size_t size );
bool HookD3D();
bool CompileShader( const char* szShader, const char * szEntrypoint, const char * szTarget, ID3D10Blob ** pBlob );
bool InitD3DHook( IDXGISwapChain* pSwapchain );
void CleanupD3D();
void Render();

// adding this code ripped off SO to find the "main window" as a fallback to RSGetViewports
struct HandleData
{
	DWORD pid;
	HWND hWnd;
};
HWND FindMainWindow( DWORD dwPID );
BOOL CALLBACK EnumWindowsCallback( HWND hWnd, LPARAM lParam );

void MainThread( void* pHandle )
{
	// Hook d3d
	if (HookD3D())
	{
		// END key to unload
		while (!GetAsyncKeyState( VK_END ));
	}

	// Cleanup and unload dll
	CleanupD3D();
	WriteMem( ogPresent, ogBytes, PRESENT_STUB_SIZE );
	VirtualFree( (void*)ogPresentTramp, 0x1000, MEM_RELEASE );
	CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, pHandle, 0, 0 );
}

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved )
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			DisableThreadLibraryCalls( hinstDLL );
			CreateThread( nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hinstDLL, 0, nullptr );
			break;
		case DLL_PROCESS_DETACH:

			break;
	}
	return TRUE;
}

bool Hook( void* pSrc, void* pDst, size_t size )
{
	DWORD dwOld;
	uintptr_t src = (uintptr_t)pSrc;
	uintptr_t dst = (uintptr_t)pDst;

	if (!VirtualProtect( pSrc, size, PAGE_EXECUTE_READWRITE, &dwOld ))
		return false;

	*(char*)src = (char)0xE9;
	*(int*)(src + 1) = (int)(dst - src - 5);

	VirtualProtect( pSrc, size, dwOld, &dwOld );
	return true;
}

bool WriteMem( void* pDst, char* pBytes, size_t size )
{
	DWORD dwOld;
	if (!VirtualProtect( pDst, size, PAGE_EXECUTE_READWRITE, &dwOld ))
		return false;

	memcpy( pDst, pBytes, PRESENT_STUB_SIZE );

	VirtualProtect( pDst, size, dwOld, &dwOld );
	return true;
}

bool HookD3D()
{
	// Create a dummy device, get swapchain vmt, hook present.
	D3D_FEATURE_LEVEL featLevel;
	DXGI_SWAP_CHAIN_DESC sd{ 0 };
	sd.BufferCount = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.Height = 800;
	sd.BufferDesc.Width = 600;
	sd.BufferDesc.RefreshRate = { 60, 1 };
	sd.OutputWindow = GetForegroundWindow();
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	HRESULT hr = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_REFERENCE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &pSwapchain, &pDevice, &featLevel, nullptr );
	if (FAILED( hr )) 
		return false;

	// Get swapchain vmt
	void** pVMT = *(void***)pSwapchain;
	
	// Get Present's address out of vmt
	ogPresent = (fnPresent)(pVMT[VMT_PRESENT]);

	// got what we need, we can release device and swapchain now
	// we'll be stealing the game's.
	safe_release( pSwapchain );
	safe_release( pDevice );

	// Create a code cave to trampoline to our hook
	// We'll try to do this close to present to make sure we'll be able to use a 5 byte jmp (important for x64)
	void* pLoc = (void*)((uintptr_t)ogPresent - 0x2000);
	void* pTrampLoc = nullptr;
	while (!pTrampLoc)
	{
		pTrampLoc = VirtualAlloc( pLoc, 1, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
		pLoc = (void*)((uintptr_t)pLoc + 0x200);
	}
	ogPresentTramp = (fnPresent)pTrampLoc;

	// write original bytes to trampoline
	// write jmp to hook
	memcpy( ogBytes, ogPresent, PRESENT_STUB_SIZE );
	memcpy( pTrampLoc, ogBytes, PRESENT_STUB_SIZE );
	
	pTrampLoc = (void*)((uintptr_t)pTrampLoc + PRESENT_STUB_SIZE);
	
	// write the jmp back into present
	*(char*)pTrampLoc = (char)0xE9;
	pTrampLoc = (void*)((uintptr_t)pTrampLoc + 1);
	uintptr_t ogPresRet = (uintptr_t)ogPresent + 5;
	*(int*)pTrampLoc = (int)(ogPresRet - (uintptr_t)pTrampLoc - 4);
	
	// write the jmp to our hook
	pTrampoline = pTrampLoc = (void*)((uintptr_t)pTrampLoc + 4);
#ifdef _WIN64
	// if x64, gazzillion byte absolute jmp
	char pJmp[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
	WriteMem( pTrampLoc, pJmp, ARRAYSIZE( pJmp ) );
	pTrampLoc = (void*)((uintptr_t)pTrampLoc + ARRAYSIZE( pJmp ));
	*(uintptr_t*)pTrampLoc = (uintptr_t)hkPresent;
#else
	// if x86, normal 0xE9 jmp
	*(char*)pTrampLoc = (char)0xE9;
	pTrampLoc = (void*)((uintptr_t)pTrampLoc + 1);
	*(int*)pTrampLoc = (uintptr_t)hkPresent - (uintptr_t)pTrampLoc - 4;
#endif

	// hook present, place a normal mid-function at the beginning of the Present function
	return Hook(ogPresent, pTrampoline, PRESENT_STUB_SIZE);
}

bool CompileShader( const char* szShader, const char * szEntrypoint, const char * szTarget, ID3D10Blob ** pBlob )
{
	ID3D10Blob* pErrorBlob = nullptr;

	auto hr = D3DCompile( szShader, strlen( szShader ), 0, nullptr, nullptr, szEntrypoint, szTarget, D3DCOMPILE_ENABLE_STRICTNESS, 0, pBlob, &pErrorBlob );
	if (FAILED( hr ))
	{
		if (pErrorBlob)
		{
			char szError[256]{ 0 };
			memcpy( szError, pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize() );
			MessageBoxA( nullptr, szError, "Error", MB_OK );
		}
		return false;
	}
	return true;
}

bool InitD3DHook( IDXGISwapChain * pSwapchain )
{
	HRESULT hr = pSwapchain->GetDevice( __uuidof(ID3D11Device), (void**)&pDevice );
	if (FAILED( hr ))
		return false;

	pDevice->GetImmediateContext( &pContext );
	pContext->OMGetRenderTargets( 1, &pRenderTargetView, nullptr );

	// If for some reason we fail to get a render target, create one.
	// This will probably never happen with a real game but maybe certain test environments... :P
	if (!pRenderTargetView)
	{
		// Get a pointer to the back buffer for the render target view
		ID3D11Texture2D* pBackbuffer = nullptr;
		hr = pSwapchain->GetBuffer( 0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackbuffer) );
		if (FAILED( hr ))
			return false;

		// Create render target view
		hr = pDevice->CreateRenderTargetView( pBackbuffer, nullptr, &pRenderTargetView );
		pBackbuffer->Release();
		if (FAILED( hr ))
			return false;

		// Make sure our render target is set.
		pContext->OMSetRenderTargets( 1, &pRenderTargetView, nullptr );
	}
	
	// initialize shaders
	ID3D10Blob* pBlob = nullptr;

	// create vertex shader
	if (!CompileShader( szShadez, "VS", "vs_5_0", &pBlob ))
		return false;

	hr = pDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &pVertexShader );
	if (FAILED( hr ))
		return false;

	// Define/create the input layout for the vertex shader
	D3D11_INPUT_ELEMENT_DESC layout[2] = {
	{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	UINT numElements = ARRAYSIZE( layout );

	hr = pDevice->CreateInputLayout( layout, numElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &pVertexLayout );
	if (FAILED( hr ))
		return false;

	safe_release( pBlob );

	// create pixel shader
	if (!CompileShader( szShadez, "PS", "ps_5_0", &pBlob ))
		return false;

	hr = pDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &pPixelShader );
	if (FAILED( hr ))
		return false;

	UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	float fWidth =  0;
	float fHeight = 0;

	// Apparently this isn't universal. Works on some games
	pContext->RSGetViewports( &numViewports, pViewports );
	
	//
	if (!numViewports || !pViewports[MAINVP].Width)
	{
		// This should be retrieved dynamically
		//HWND hWnd0 = FindWindowA( "W2ViewportClass", nullptr );
		HWND hWnd = FindMainWindow( GetCurrentProcessId() );
		RECT rc{ 0 };
		if (!GetClientRect( hWnd, &rc ))
			return false;

		//fWidth = 1600.0f;
		//fHeight = 900.0f;
		fWidth = (float)rc.right;
		fHeight = (float)rc.bottom;

		// Setup viewport
		pViewports[MAINVP].Width = (float)fWidth;
		pViewports[MAINVP].Height = (float)fHeight;
		pViewports[MAINVP].MinDepth = 0.0f;
		pViewports[MAINVP].MaxDepth = 1.0f;

		// Set viewport to context
		pContext->RSSetViewports( 1, pViewports );
	}
	else
	{
		fWidth = (float)pViewports[MAINVP].Width;
		fHeight = (float)pViewports[MAINVP].Height;
	}
	// Create the constant buffer
	D3D11_BUFFER_DESC bd{ 0 };
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof( ConstantBuffer );
	bd.Usage = D3D11_USAGE_DEFAULT;
	
	// Setup orthographic projection
	mOrtho = XMMatrixOrthographicLH( fWidth, fHeight, 0.0f, 1.0f );
	ConstantBuffer cb;
	cb.mProjection = mOrtho;
	
	D3D11_SUBRESOURCE_DATA sr{ 0 };
	sr.pSysMem = &cb;
	hr = pDevice->CreateBuffer( &bd, &sr, &pConstantBuffer );
	if (FAILED( hr ))
		return false;

	// Create a triangle to render
	// Create a vertex buffer, start by setting up a description.
	ZeroMemory( &bd, sizeof( bd ) );
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = 3 * sizeof( Vertex );
	bd.StructureByteStride = sizeof( Vertex );

	// left and top edge of window
	float left = fWidth / -2;
	float top = fHeight / 2;

	// Width and height of triangle
	float w = 50;
	float h = 50;
	
	// Center position of triangle, this should center it in the screen.
	float fPosX = -1 * left;
	float fPosY = top;

	// Setup vertices of triangle
	Vertex pVerts[3] = {
		{ XMFLOAT3( left + fPosX,			top - fPosY + h / 2,	1.0f ),	XMFLOAT4( 1.0f, 0.0f, 0.0f, 1.0f ) },
		{ XMFLOAT3( left + fPosX + w / 2,	top - fPosY - h / 2,	1.0f ),	XMFLOAT4( 0.0f, 0.0f, 1.0f, 1.0f ) },
		{ XMFLOAT3( left + fPosX - w / 2,	top - fPosY - h / 2,	1.0f ),	XMFLOAT4( 0.0f, 1.0f, 0.0f, 1.0f ) },
	};	  

	// create the buffer.
	ZeroMemory( &sr, sizeof( sr ) );
	sr.pSysMem = &pVerts;
	hr = pDevice->CreateBuffer( &bd, &sr, &pVertexBuffer );
	if (FAILED( hr ))
		return false;

	// Create an index buffer
	ZeroMemory( &bd, sizeof( bd ) );
	ZeroMemory( &sr, sizeof( sr ) );

	UINT pIndices[3] = { 0, 1, 2 };
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( UINT ) * 3;
	bd.StructureByteStride = sizeof( UINT );

	sr.pSysMem = &pIndices;
	hr = pDevice->CreateBuffer( &bd, &sr, &pIndexBuffer );
	if (FAILED( hr ))
		return false;

	return true;
}

void CleanupD3D()
{
	safe_release( pVertexBuffer );
	safe_release( pIndexBuffer );
	safe_release( pConstantBuffer );
	safe_release( pPixelShader );
	safe_release( pVertexShader );
	safe_release( pVertexLayout );
}

void Render()
{	
	// Make sure our render target is set.
	pContext->OMSetRenderTargets( 1, &pRenderTargetView, nullptr );

	// Update view
	ConstantBuffer cb;
	cb.mProjection = XMMatrixTranspose( mOrtho );
	pContext->UpdateSubresource( pConstantBuffer, 0, nullptr, &cb, 0, 0 );
	pContext->VSSetConstantBuffers( 0, 1, &pConstantBuffer );

	// Make sure the input assembler knows how to process our verts/indices
	UINT stride = sizeof( Vertex );
	UINT offset = 0;
	pContext->IASetVertexBuffers( 0, 1, &pVertexBuffer, &stride, &offset );
	pContext->IASetInputLayout( pVertexLayout );
	pContext->IASetIndexBuffer( pIndexBuffer, DXGI_FORMAT_R32_UINT, 0 );
	pContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	
	// Set the shaders we need to render our triangle
	pContext->VSSetShader( pVertexShader, nullptr, 0 );
	pContext->PSSetShader( pPixelShader, nullptr, 0 );

	// Set viewport to context
	pContext->RSSetViewports( 1, pViewports );

	// Draw our triangle
	pContext->DrawIndexed( 3, 0, 0 );
}

HRESULT __stdcall hkPresent( IDXGISwapChain * pThis, UINT SyncInterval, UINT Flags )
{
	pSwapchain = pThis;

	if (!pDevice)
	{
		if (!InitD3DHook( pThis ))
			return false;
	}

	Render();
	return ogPresentTramp( pThis, SyncInterval, Flags );
}

HWND FindMainWindow( DWORD dwPID )
{
	HandleData handleData{ 0 };
	handleData.pid = dwPID;
	EnumWindows( EnumWindowsCallback, (LPARAM)&handleData );
	return handleData.hWnd;
}

BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam)
{
	HandleData& data = *(HandleData*)lParam;
	DWORD pid = 0;
	GetWindowThreadProcessId( hWnd, &pid );
	if (pid == data.pid && GetWindow( hWnd, GW_OWNER ) == HWND( 0 ) && IsWindowVisible( hWnd ))
	{
		data.hWnd = hWnd;
		return FALSE;
	}

	return TRUE;
}