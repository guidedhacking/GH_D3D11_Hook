# GH D3D11 Hook

Barebones D3D11 hook.
x86/x64 Compatible

![screenshot](https://github.com/guided-hacking/GH_D3D11_Hook/blob/master/ss.png "FarCry5 Example")

[BareBones D3D11 Hook Thread on GH](https://guidedhacking.com/threads/d3d11-barebones-hook-poc.11939/)

This is about the simplest example I could create for hooking DirectX 11.
The code is heavily commented and doesn't rely on any external libraries.
As long as you have a recent version of the Windows SDK then this code should run.

The example is a DLL that you inject into a game using DirectX 11 and it should render a triangle in the top left corner of the screen.

The code creates a dummy device and swapchain to get the address of Present out of the dummy swapchain's virtual method table. Then we create a simple trampoline hook to detour Present and render our triangle.

While the code isn't extremely elegant, it's not meant to be.
This is just a simple PoC to demonstrate one way, a fairly decent way imo, of hooking
direct3d 11 and rendering our own simple geometry.

This project is a Direct3D 11 hook, which is essentially a way to intercept and manipulate the rendering of 3D graphics in a game or other 3D application. This is a powerful tool for game modding, reverse engineering, and cheat development.

Features
--------

The GH_D3D11_Hook project provides the following features:

1.  DirectX Hooking: This project uses a technique known as "hooking" to intercept the DirectX 11 API calls, allowing you to manipulate the rendering process.

2.  Shader Compilation: The project includes functionality for compiling shaders, which are programs that run on the GPU to perform graphics calculations.

3.  Rendering: The project includes code for rendering a basic triangle using the hooked DirectX 11 API. This serves as an example of how you can use the hook to modify the rendering process.

4.  Memory Management: The project includes robust memory management, with safe release macros for DirectX objects.

Code Overview
-------------

The main file of the project is `DllMain.cpp`, which contains the implementation of the DirectX 11 hook. Here's a brief overview of some key parts of the code:

-   `hkPresent`: This is the hooked version of the `Present` function, which is a DirectX function that updates the screen with the rendered frame. This function is where the magic happens: it's where we intercept the `Present` call and add our own rendering code.

-   `HookD3D`: This function sets up the hook by creating a dummy DirectX device and swap chain, getting the virtual method table (VMT) of the swap chain, and replacing the `Present` function in the VMT with our own function.

-   `InitD3DHook`: This function initializes the hook by getting the DirectX device and context, setting up the render target view, and initializing the shaders and buffers for rendering.

-   `Render`: This function is where we add our own rendering code. In this case, it renders a simple triangle.

-   `CompileShader`: This function compiles a shader from source code.

-   `FindMainWindow` and `EnumWindowsCallback`: These functions are used to find the main window of the process, which is needed to create the dummy DirectX device and swap chain.


Frequently Asked Questions
--------------------------

Q: What is DirectX?

A: DirectX is a collection of APIs (Application Programming Interfaces) for handling tasks related to multimedia, especially game programming and video, on Microsoft platforms.

Q: What is a DirectX hook?

A: A DirectX hook is a technique that allows you to intercept and manipulate the DirectX API calls made by an application. This can be used for a variety of purposes, such as modifying the rendering process, capturing frames, or injecting custom shaders.

Q: What is a shader?

A: A shader is a type of program that is run on the GPU (Graphics Processing Unit). Shaders are used to perform calculations related to rendering, such as vertex transformations, lighting calculations, or pixel coloring.

Q: What is a VMT (Virtual Method Table)?

A: A VMT, or Virtual Method Table, is a mechanism used in C++ to support dynamic dispatch (or runtime method binding). Each class with virtual functions (or an inherited interface) has its own VMT. A VMT is essentially an array of function pointers that the compiler creates for us. When we declare a class with some virtual methods, the compiler silently writes some code to create the VMT for that class.

Prerequisites
-------------

To use this code, you should have a good understanding of C++ and Windows programming. You should also be familiar with DirectX 11 and graphics programming concepts. Knowledge of reverse engineering techniques and assembly language will also be helpful.

End Goal
--------

The end goal of this project is to provide a robust and flexible DirectX 11 hook that can be used as a starting point for game modding, reverse engineering, or cheat development. The provided code demonstrates how to set up the hook and use it to modify the rendering process, but it can be extended to perform more complex tasks.

Key Pieces of Code
------------------

The key pieces of this project are the hooking mechanism (`HookD3D` function), the hooked `Present` function (`hkPresent`), and the rendering code in the `Render` function. These pieces of code demonstrate the core functionality of the DirectX 11 hook.

Associated Resources​
---------------------

-   [Source Code - D3D11 X64 Present Hook](https://guidedhacking.com/threads/d3d11-x64-present-hook.15283/)
-   [Source Code - D3D11 Barebones hook PoC](https://guidedhacking.com/threads/d3d11-barebones-hook-poc.11939/)
-   [Source Code - D3D11 SWBF2 Cheats ( 2017 )](https://guidedhacking.com/threads/star-wars-battlefront-ii-hacks-swbf2-cheats-2017.15348/)
-   [DirectX11 Hook and Logger](https://guidedhacking.com/threads/directx11-hook-and-logger.11910/)
