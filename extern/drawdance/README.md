# NAME

![Drawdance - A Drawpile Client](logo.png)

# SYNOPSIS

Drawdance is an alternative client for [Drawpile, the collaborative drawing program](https://github.com/drawpile/Drawpile). It is intended to run on desktop, phones, tablets and in the browser.

Currently supported are Linux and the browser. The initial goal is to provide a client for viewing, support for drawing and other platforms may be added later.

# DESCRIPTION

There'll be a proper description here eventually.

## Building

### Linux

Until there's a proper writeup, check out [scripts/cmake-presets](scripts/cmake-presets) for settings that work for me on Linux. You need gcc or clang, CMake, SDL2 and Ninja for that to have a chance of working out of the box. For the browser, you also need Emscripten installed and loaded into your environment.

### Windows

On windows, it's highly recommended to install dependencies with [vcpkg](https://github.com/microsoft/vcpkg).

```
vcpkg install qt5-base libpng libjpeg-turbo kf5archive curl --triplet=x64-windows-static
```

Then you can build the libraries with the following commands:

```
mkdir build
cd build

cmake -DCMAKE_TOOLCHAIN_FILE=[PATH TO VCPKG]/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DBUILD_APPS=OFF -DUSE_CLANG_TIDY=OFF -DXML_IMPL=QT -DZIP_IMPL=KARCHIVE -DLINK_WITH_LIBM=OFF -DCMAKE_BUILD_TYPE=Release -DUSE_ADDRESS_SANITIZER=OFF -G Ninja ../

cmake --build .
```

## Running

On Linux, run the built executable from the root directory of the repository (the directory that this file is in.)

For the browser, you can use the [server proxy](server) and point it at your build directory. From the application, connect to `ws://localhost:27751`. Note that if you're not connecting to localhost, you need to have TLS certificates, the browser won't let you use an unsecured WebSocket connection.

## Testing

To run the tests, use `ctest --test-dir YOUR-BUILD-DIRECTORY` after building. They don't work for Emscripten at this point.

# LICENSE

Drawdance is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version. See the [LICENSE file](LICENSE) for the full license text.

You may also use original Drawdance code not based on other sources under the MIT license. You **cannot** relicense non-original parts! Therefore the software as a whole remains GPLv3 or later. See documentation in the source code itself for which parts are original and which are based on other sources. See the [LICENSE-FOR-ORIGINAL-CODE file](LICENSE-FOR-ORIGINAL-CODE) for the full license text.

The Drawdance logo is Copyright 2022 askmeaboutloom and licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).

# SEE ALSO

<https://drawpile.net/> - the official Drawpile website.

<https://github.com/drawpile/Drawpile> - Drawpile on GitHub.
