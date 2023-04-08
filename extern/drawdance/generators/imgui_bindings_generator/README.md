This generates the Dear ImGui Lua bindings in [appdrawdance/drawdance\_lua/imgui\_gen.cpp](../../appdrawdance/drawdance_lua/imgui_gen.cpp). It requires Perl version 5.20 or later and is kinda ugly, sorry. It totally works though.

To run it, first fetch the ImGui git repo using [scripts/get-imgui](../../scripts/get-imgui).

Then grab [cimgui](https://github.com/cimgui/cimgui) and generate the outputs based on the ImGui version used in Drawdance:

```
git clone https://github.com/cimgui/cimgui.git
cd cimgui/generator
IMGUI_PATH=/path/to/drawdance/3rdparty/imgui_git ./generator.sh
```

Then run the generator:

```
./generate_imgui_bindings /path/to/cimgui/generator/output
```

Finally, run clang-format so the results are pretty again and rebuild.

Everything involved is under the MIT license, see the files in this directory for details.

This generator is **not** based on the Lua bindings at <https://github.com/patrickriordan/imgui_lua_bindings>. I've avoided them due to the questionable license they are under.
