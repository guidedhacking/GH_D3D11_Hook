# GH D3D11 Hook

Barebones D3D11 hook.

x86/x64 Compatible

This is about the simplest example I could create for hooking DirectX 11.
The code is heavily commented and doesn't rely on any external libraries.
As long as you have a recent version of the Windows SDK then this code should run.

The example is a DLL that you inject into a game using DirectX 11 and it should render
a triangle in the top left corner of the screen.

The code creates a dummy device and swapchain to get the address of Present out of
the dummy swapchain's virtual method table. Then we create a simple trampoline hook
to detour Present and render our triangle.

While the code isn't extremely elegant, it's not meant to be.
This is just a simple PoC to demonstrate one way, a fairly decent way imo, of hooking
direct3d 11 and rendering our own simple geometry.

![screenshot](https://github.com/guided-hacking/GH_D3D11_Hook/blob/master/ss.png "FarCry5 Example")

[BareBones D3D11 Hook Thread on GH](https://guidedhacking.com/threads/d3d11-barebones-hook-poc.11939/)
