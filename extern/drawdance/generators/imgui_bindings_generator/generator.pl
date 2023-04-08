#!/usr/bin/env perl
# Copyright (c) 2022 askmeaboutloom
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
use 5.020;
use warnings;
use experimental qw(signatures);
use Encode qw(decode);
use FindBin;
use File::Spec::Functions qw(catdir catfile);
use List::Util qw(any min max sum);
use JSON::PP qw(decode_json);


if (@ARGV != 1) {
    die "Usage: $0 PATH_TO_CIMGUI_OUTPUT_DIRECTORY\n";
}

my $input_dir = $ARGV[0];
if (!-e $input_dir) {
    die "Given cimgui output directory not found: '$input_dir'\n";
}

my $base_dir = $FindBin::Bin;
require(catfile($base_dir, 'overloads.pl'));


sub slurp($path) {
    open my $fh, '<', $path or die "Can't open '$path': $!\n";
    my $content = do {
        local $/; # slurp mode
        decode('UTF-8', scalar <$fh>);
    };
    close $fh or die "Error closing '$path': $!\n";
    return $content;
}

sub decode_json_file($filename) {
    my $path = catfile($input_dir, $filename);
    return decode_json(slurp($path));
}

sub read_include($filename) {
    my $path = catfile($base_dir, $filename);
    return slurp($path);
}

sub indent($spaces, $string) {
    return $string unless $string =~ /\S/;
    my $indent = ' ' x $spaces;
    return $string =~ s/^/$indent/mgr;
}


sub generate_enum($name, $values) {
    $name =~ s/_+\z//;
    print qq/IMGUILUA_ENUM($name)\n/;
    print qq/static void ImGuiLua_RegisterEnum$name(lua_State *L)\n/;
    print qq/{\n/;
    for my $value (@$values) {
        my $key = $value->{name} =~ s/\A\Q$name\E_*//r;
        print qq/    ImGuiLua_SetEnumValue(L, "$key", $value->{value});\n/;
    }
    print qq/}\n\n/;
}


sub grouped_definitions($definitions) {
    my %grouped;
    for my $def (map { @$_ } values %$definitions) {
        my $key = join '::', grep { defined }
                  @{$def}{qw(namespace stname funcname)};
        push @{$grouped{$key} ||= []}, $def;
    }
    return map { [sort { $a->{args} cmp $b->{args} } @$_] }
           map { $grouped{$_} } sort keys %grouped;
}

sub make_function_title($def) {
    my @parts;
    push @parts, $def->{namespace} if $def->{namespace};
    push @parts, $def->{stname}    if $def->{stname};
    push @parts, $def->{destructor} ? "~$def->{stname}" : $def->{funcname};
    my $name = join '::', @parts;
    my $type = $def->{stname} ? 'method' : 'function';
    return $def->{is_static_function} ? "static $type $name" : "$type $name";
}


my %begin_to_end = qw(
    ImGui::Begin                   ImGui::End
    ImGui::BeginChild              ImGui::EndChild
    ImGui::BeginGroup              ImGui::EndGroup
    ImGui::BeginCombo              ImGui::EndCombo
    ImGui::BeginListBox            ImGui::EndListBox
    ImGui::BeginMenuBar            ImGui::EndMenuBar
    ImGui::BeginMainMenuBar        ImGui::EndMainMenuBar
    ImGui::BeginMenu               ImGui::EndMenu
    ImGui::BeginTooltip            ImGui::EndTooltip
    ImGui::BeginPopup              ImGui::EndPopup
    ImGui::BeginPopupModal         ImGui::EndPopup
    ImGui::BeginPopupContextItem   ImGui::EndPopup
    ImGui::BeginPopupContextWindow ImGui::EndPopup
    ImGui::BeginPopupContextVoid   ImGui::EndPopup
    ImGui::BeginTable              ImGui::EndTable
    ImGui::BeginTabBar             ImGui::EndTabBar
    ImGui::BeginTabItem            ImGui::EndTabItem
    ImGui::BeginDragDropSource     ImGui::EndDragDropSource
    ImGui::BeginDragDropTarget     ImGui::EndDragDropTarget
    ImGui::BeginChildFrame         ImGui::EndChildFrame
);

my %end_to_begins;
for my $begin (sort keys %begin_to_end) {
    my $end = $begin_to_end{$begin};
    push @{$end_to_begins{$end} ||= []}, $begin;
}

my %end_indexes;
{
    my @keys = sort keys %end_to_begins;
    for my $i (0 .. $#keys) {
        $end_indexes{$keys[$i]} = $i + 1;
    }
}


my %exclusions = map { $_ => 1 } (
    'function ImGui::BulletText',      # custom implementation
    'function ImGui::BulletTextV',     # custom implementation
    'function ImGui::EndFrame',        # left to the application
    'function ImGui::GetID',           # custom implementation
    'function ImGui::GetIO',           # custom implementation
    'function ImGui::LabelText',       # custom implementation
    'function ImGui::LabelTextV',      # custom implementation
    'function ImGui::NewFrame',        # left to the application
    'function ImGui::PopStyleColor',  # custom implementation
    'function ImGui::PopStyleVar',    # custom implementation
    'function ImGui::PushID',          # custom implementation
    'function ImGui::PushStyleColor',  # custom implementation
    'function ImGui::PushStyleVar',    # custom implementation
    'function ImGui::Render',          # left to the application
    'function ImGui::Text',            # custom implementation
    'function ImGui::TextColored',     # custom implementation
    'function ImGui::TextColoredV',    # custom implementation
    'function ImGui::TextDisabled',    # custom implementation
    'function ImGui::TextDisabledV',   # custom implementation
    'function ImGui::TextUnformatted', # custom implementation
    'function ImGui::TextV',           # custom implementation
    'function ImGui::TextWrapped',     # custom implementation
    'function ImGui::TextWrappedV',    # custom implementation
    'function ImGui::Value',           # not useful (string helper)
    # I don't think these are useful from Lua.
    'overload bool ImGui_MenuItem(const char*, const char*, bool*, bool)',
    'overload bool ImGui_Selectable(const char*, bool*, ImGuiSelectableFlags, const ImVec2)',
);

my %type_aliases = (
    ImGuiCol => 'int',
    ImGuiCond => 'int',
    ImGuiDataType => 'int',
    ImGuiDir => 'int',
    ImGuiDockNodeFlags => 'int',
    ImGuiKey => 'int',
    ImGuiNavInput => 'int',
    ImGuiMouseButton => 'int',
    ImGuiMouseCursor => 'int',
    ImGuiSortDirection => 'int',
    ImGuiStyleVar => 'int',
    ImGuiTableBgTarget => 'int',
    ImDrawFlags => 'int',
    ImDrawListFlags => 'int',
    ImFontAtlasFlags => 'int',
    ImGuiBackendFlags => 'int',
    ImGuiButtonFlags => 'int',
    ImGuiColorEditFlags => 'int',
    ImGuiConfigFlags => 'int',
    ImGuiComboFlags => 'int',
    ImGuiDragDropFlags => 'int',
    ImGuiFocusedFlags => 'int',
    ImGuiHoveredFlags => 'int',
    ImGuiInputTextFlags => 'int',
    ImGuiKeyModFlags => 'int',
    ImGuiPopupFlags => 'int',
    ImGuiSelectableFlags => 'int',
    ImGuiSliderFlags => 'int',
    ImGuiTabBarFlags => 'int',
    ImGuiTabItemFlags => 'int',
    ImGuiTableFlags => 'int',
    ImGuiTableColumnFlags => 'int',
    ImGuiTableRowFlags => 'int',
    ImGuiTreeNodeFlags => 'int',
    ImGuiViewportFlags => 'int',
    ImGuiWindowFlags => 'int',
    ImGuiID => 'unsigned int',
);

for my $key (keys %type_aliases) {
    $type_aliases{"const $key"} = $type_aliases{$key};
}


my %function_returns = (
    'void' => {
        retcount => 0,
        assign => sub ($call) {
            return qq/$call;\n/;
        },
        before_begin => sub ($fullname, $spaces_ref) {
            return '';
        },
        after_begin => sub ($fullname) {
            return '';
        },
    },
    'bool' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/bool RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushboolean(L, RETVAL);\n/;
        },
        before_begin => sub ($fullname, $spaces_ref) {
            return '' if $fullname =~ /\AImGui::Begin(?:Child)?\z/;
            $$spaces_ref += 4;
            return qq/if (RETVAL) {\n/;
        },
        after_begin => sub ($fullname) {
            return '' if $fullname =~ /\AImGui::Begin(?:Child)?\z/;
            return qq/}\n/;
        },
    },
    'const char*' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/const char *RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushstring(L, RETVAL);\n/;
        },
    },
    'int' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/int RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));\n/;
        },
    },
    'unsigned int' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/unsigned int RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));\n/;
        },
    },
    'ImU32' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/ImU32 RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushinteger(L, static_cast<lua_Integer>(RETVAL));\n/;
        },
    },
    'float' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/float RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushnumber(L, static_cast<lua_Number>(RETVAL));\n/;
        },
    },
    'double' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/double RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/lua_pushnumber(L, static_cast<lua_Number>(RETVAL));\n/;
        },
    },
    'ImVec2' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/ImVec2 RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/ImGuiLua_PushImVec2(L, RETVAL);\n/;
        },
    },
    'ImVec4' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/ImVec4 RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/ImGuiLua_PushImVec4(L, RETVAL);\n/;
        },
    },
    'ImGuiViewport*' => {
        retcount => 1,
        assign => sub ($call) {
            return qq/ImGuiViewport *RETVAL = $call;\n/;
        },
        out => sub () {
            return qq/ImGuiViewport **pp = static_cast<ImGuiViewport **>(lua_newuserdatauv(L, sizeof(*pp), 0));\n/
                 . qq/*pp = RETVAL;\n/
                 . qq/luaL_setmetatable(L, "ImGuiViewport");\n/;
        },
    },
);

sub translate_function_return($func, $ret) {
    my $actual_type = $type_aliases{$ret} // $ret;
    if (exists $function_returns{$actual_type}) {
        $func->{ret} = $function_returns{$actual_type};
    }
    else {
        die "Unknown return type '$actual_type' ($ret)\n";
    }
}


my %function_args = (
    'bool' => {
        retcount => 0,
        overload => 'b',
        declare => sub ($arg) {
            return qq/bool $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = lua_toboolean(L, $arg->{index});\n/;
        },
    },
    'bool*' => {
        retcount => 0,
        overload => 'bP',
        declare => sub ($arg) {
            return qq/bool *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/if (lua_type(L, $arg->{index}) == LUA_TNIL) {\n/
                 . qq/    $arg->{name} = NULL;\n/
                 . qq/}\n/
                 . qq/else {\n/
                 . qq/    $arg->{name} = static_cast<bool *>(luaL_checkudata(L, $arg->{index}, "ImGuiLuaBoolRef"));\n/
                 . qq/}\n/;
        },
    },
    'const char*' => {
        retcount => 0,
        overload => 's',
        declare => sub ($arg) {
            return qq/const char *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = lua_tostring(L, $arg->{index});\n/;
        },
    },
    'int' => {
        retcount => 0,
        overload => 'i',
        declare => sub ($arg) {
            return qq/int $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<int>(luaL_checkinteger(L, $arg->{index}));\n/;
        },
    },
    'int*' => {
        retcount => 0,
        overload => 'fP',
        declare => sub ($arg) {
            return qq/int *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<int *>(luaL_checkudata(L, $arg->{index}, "ImGuiLuaIntRef"));\n/;
        },
    },
    'unsigned int' => {
        retcount => 0,
        overload => 'u',
        declare => sub ($arg) {
            return qq/unsigned int $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<unsigned int>(luaL_checkinteger(L, $arg->{index}));\n/;
        },
    },
    'ImU32' => {
        retcount => 0,
        overload => 'u32',
        declare => sub ($arg) {
            return qq/ImU32 $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<ImU32>(luaL_checkinteger(L, $arg->{index}));\n/;
        },
    },
    'float' => {
        retcount => 0,
        overload => 'f',
        declare => sub ($arg) {
            return qq/float $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<float>(luaL_checknumber(L, $arg->{index}));\n/;
        },
    },
    'float*' => {
        retcount => 0,
        overload => 'fP',
        declare => sub ($arg) {
            return qq/float *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<float *>(luaL_checkudata(L, $arg->{index}, "ImGuiLuaFloatRef"));\n/;
        },
    },
    'double' => {
        retcount => 0,
        overload => 'd',
        declare => sub ($arg) {
            return qq/double $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<double>(luaL_checknumber(L, $arg->{index}));\n/;
        },
    },
    'ImVec2' => {
        retcount => 0,
        overload => 'v2',
        declare => sub ($arg) {
            return qq/ImVec2 $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = ImGuiLua_ToImVec2(L, $arg->{index});\n/;
        },
    },
    'ImVec4' => {
        retcount => 0,
        overload => 'v4',
        declare => sub ($arg) {
            return qq/ImVec4 $arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = ImGuiLua_ToImVec4(L, $arg->{index});\n/;
        },
    },
    'ImVec2*' => {
        retcount => 0,
        overload => 'v2',
        declare => sub ($arg) {
            return qq/ImVec2 *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<ImVec2 *>(luaL_checkudata(L, $arg->{index}, "ImVec2"));\n/;
        },
    },
    'ImVec4*' => {
        retcount => 0,
        overload => 'v4',
        declare => sub ($arg) {
            return qq/ImVec4 *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<ImVec4 *>(luaL_checkudata(L, $arg->{index}, "ImVec4"));\n/;
        },
    },
    'ImGuiIO*' => {
        retcount => 0,
        overload => 'io',
        declare => sub ($arg) {
            return qq/ImGuiIO *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = *static_cast<ImGuiIO **>(luaL_checkudata(L, $arg->{index}, "ImGuiIO"));\n/;
        },
    },
    'ImGuiViewport*' => {
        retcount => 0,
        overload => 'iv',
        declare => sub ($arg) {
            return qq/ImGuiViewport *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = *static_cast<ImGuiViewport **>(luaL_checkudata(L, $arg->{index}, "ImGuiViewport"));\n/;
        },
    },
    'const ImGuiViewport*' => {
        retcount => 0,
        overload => 'ivC',
        declare => sub ($arg) {
            return qq/const ImGuiViewport *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = *static_cast<ImGuiViewport **>(luaL_checkudata(L, $arg->{index}, "ImGuiViewport"));\n/;
        },
    },
    'ImGuiWindowClass*' => {
        retcount => 0,
        overload => 'iw',
        declare => sub ($arg) {
            return qq/ImGuiWindowClass *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<ImGuiWindowClass *>(luaL_checkudata(L, $arg->{index}, "ImGuiWindowClass"));\n/;
        },
    },
    'const ImGuiWindowClass*' => {
        retcount => 0,
        overload => 'iwC',
        declare => sub ($arg) {
            return qq/const ImGuiWindowClass *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<ImGuiWindowClass *>(luaL_checkudata(L, $arg->{index}, "ImGuiWindowClass"));\n/;
        },
    },
    'ImGuiLuaBoolRef*' => {
        retcount => 0,
        overload => 'bR',
        declare => sub ($arg) {
            return qq/bool *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<bool *>(luaL_checkudata(L, $arg->{index}, "ImGuiLuaBoolRef"));\n/
        },
    },
    'ImGuiLuaIntRef*' => {
        retcount => 0,
        overload => 'iR',
        declare => sub ($arg) {
            return qq/int *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<int *>(luaL_checkudata(L, $arg->{index}, "ImGuiLuaIntRef"));\n/
        },
    },
    'ImGuiLuaFloatRef*' => {
        retcount => 0,
        overload => 'fR',
        declare => sub ($arg) {
            return qq/float *$arg->{name};\n/;
        },
        in => sub ($arg) {
            return qq/$arg->{name} = static_cast<float *>(luaL_checkudata(L, $arg->{index}, "ImGuiLuaFloatRef"));\n/
        },
    },
    'ImGuiLuaStringRef*' => {
        retcount => 0,
        overload => 'sR',
        declare => sub ($arg) {
            return '';
        },
        in => sub ($arg) {
            return qq/luaL_checkudata(L, $arg->{index}, "ImGuiLuaStringRef");\n/
        },
    },
);

for my $type (qw(bool int float double ImVec2 ImVec4)) {
    $function_args{"const $type"} = $function_args{$type};
}

sub translate_function_arg($func, $i, $type, $name, $default) {
    my $actual_type = $type_aliases{$type} // $type;
    if (exists $function_args{$actual_type}) {
        push @{$func->{args}}, {
            index   => $i + 1,
            name    => $name,
            default => $default,
            %{$function_args{$actual_type}},
        };
    }
    else {
        die "Unknown argument type '$actual_type' ($type)\n";
    }
}


sub translate_function($def) {
    my $name = $def->{funcname};
    my $call = "$name$def->{call_args}";
    if ($def->{namespace}) {
        $name = "$def->{namespace}_$name";
        $call = "$def->{namespace}::$call";
    }
    elsif ($def->{stname}) {
        $name = "$def->{stname}_$name";
        my $invoker = $def->{argsT}[$def->{nonUDT} ? 1 : 0]{name};
        $call = "$invoker->$call";
    }

    my $func = {
        namespace => $def->{namespace} || '',
        stname    => $def->{stname} || '',
        funcname  => $def->{funcname},
        name      => $name,
        call      => $call,
        ret       => undef,
        args      => [],
    };

    my $ret  = $def->{ret};
    my $args = $def->{argsT};

    # The cimgui generator translates functions returning a class instance
    # to functions returning void and taking an out argument pointer as
    # their first parameter. Here we undo that transformation again.
    if ($def->{nonUDT}) {
        if ($ret ne 'void') {
            die "Unexpected return type for nonUDT function: $ret";
        }
        my $first = shift @$args;
        if ($first->{type} !~ /\*\z/) {
            die "Unexpected first arg type for nonUDT function: $first->{type}";
        }
        $ret = $first->{type} =~ s/\*\z//r;
    }

    my $argtypes  = join ', ', map { $_->{type} } @$args;
    my $signature = "$ret $name($argtypes)";
    if ($exclusions{"overload $signature"}) {
        print "// $signature is excluded\n\n";
        return;
    }

    my $ok = eval {
        translate_function_return($func, $ret);
        my $defaults = $def->{defaults} || {};
        for my $i (0 .. $#$args) {
            my ($type, $name) = @{$args->[$i]}{qw(type name)};
            translate_function_arg($func, $i, $type, $name, $defaults->{$name});
        }
        $func->{max_argc} = scalar @$args;
        $func->{min_argc} = max 0, map { $_->{index} }
                                   grep { !defined $_->{default} }
                                   @{$func->{args}};
        1;
    };

    if ($ok) {
        my $overload_suffix = join '', map { $_->{overload} } @{$func->{args}};
        $func->{overload_name} = join '_', $name, $overload_suffix;
        $func->{signature}     = $signature;
        return $func;
    }
    else {
        my $error     = "$@" =~ s/\s*\z//r;
        print "// $signature: $error\n\n";
        return;
    }
}

sub write_function($func, $overload) {
    my $method = !$func->{namespace};
    my $prefix = $method ? $func->{stname} : $func->{namespace};
    my $fullname = "${prefix}::$func->{funcname}";
    if (!$overload) {
        if ($method) {
            print qq/IMGUILUA_METHOD($fullname)\n/;
        }
        else {
            print qq/IMGUILUA_FUNCTION($fullname)\n/;
        }
    }

    my $name = $overload ? $func->{overload_name} : $func->{name};
    print qq/static int $name(lua_State *L)\n/;
    print qq/{\n/;

    my ($min_argc, $max_argc) = @{$func}{qw/min_argc max_argc/};
    if ($min_argc == $max_argc) {
        print qq/    IMGUILUA_CHECK_ARGC($min_argc);\n/;
    }
    elsif ($min_argc == 0) {
        print qq/    IMGUILUA_CHECK_ARGC_MAX($max_argc);\n/;
    }
    else {
        print qq/    IMGUILUA_CHECK_ARGC_BETWEEN($min_argc, $max_argc);\n/;
    }

    for my $arg (@{$func->{args}}) {
        print indent(4, $arg->{declare}->($arg)) if $arg->{declare};
        if (defined $arg->{default}) {
            print qq/    if (ARGC < $arg->{index}) {\n/;
            print qq/        $arg->{name} = $arg->{default};\n/;
            print qq/    }\n/;
            print qq/    else {\n/;
            print indent(8, $arg->{in}->($arg));
            print qq/    }\n/;
        }
        elsif ($arg->{in}) {
            print indent(4, $arg->{in}->($arg));
        }
    }

    if (exists $end_to_begins{$fullname}) {
        my $begins = join ', ', @{$end_to_begins{$fullname}};
        my $i      = $end_indexes{$fullname};
        print indent(4, qq/IMGUILUA_END(L, $fullname, $i, $begins);\n/);
    }

    print indent(4, $func->{ret}{assign}->($func->{call}));

    if (exists $begin_to_end{$fullname}) {
        my $end    = $begin_to_end{$fullname};
        my $i      = $end_indexes{$end};
        my $spaces = 4;
        print indent(4, $func->{ret}{before_begin}->($fullname, \$spaces));
        print indent($spaces, qq/IMGUILUA_BEGIN(L, $fullname, $end, $i);\n/);
        print indent(4, $func->{ret}{after_begin}->($fullname));
    }

    print indent(4, $func->{ret}{out}->()) if $func->{ret}{out};
    for my $arg (@{$func->{args}}) {
        print indent(4, $arg->{out}->($arg)) if $arg->{out};
    }

    my $retcount = sum map { $_->{retcount} } $func->{ret}, @{$func->{args}};
    print qq/    return $retcount;\n/;
    print qq/}\n\n/;

    return $name;
}

sub resolve_overload_by_argument_count(@overloads) {
    my %argc_to_name;
    for my $func (@overloads) {
        my ($min_argc, $max_argc) = @{$func}{qw/min_argc max_argc/};
        for my $i ($min_argc .. $max_argc) {
            return if exists $argc_to_name{$i};
            $argc_to_name{$i} = $func->{overload_name};
        }
    }

    my %name_to_argcs;
    for my $argc (keys %argc_to_name) {
        my $name = $argc_to_name{$argc};
        push @{$name_to_argcs{$name} ||= []}, 0 + $argc;
    }

    my @argcs = sort { $a <=> $b } map { @$_ } values %name_to_argcs;

    return sub (@names) {
        my $code = qq/int ARGC = lua_gettop(L);\n/ .
                   qq/switch (ARGC) {\n/;

        for my $name (@names) {
            for my $i (@{$name_to_argcs{$name}}) {
                $code .= qq/case $i:\n/;
            }
            $code .= qq/    return $name(L);\n/;
        }

        return $code
             . qq/default:\n/
             . qq/    return luaL_error(L, "Wrong number of arguments: got %d, wanted between %d and %d", ARGC, $argcs[0], $argcs[-1]);\n/
             . qq/}\n/;
    };
}

sub generate_overload_resolution(@overloads) {
    my @signatures = map { $_->{signature} } @overloads;
    my $resolution = ImGuiLua::Overloads::get(@signatures)
                  || resolve_overload_by_argument_count(@overloads);
    if ($resolution) {
        my ($namespace, $stname, $funcname, $name) = @{$overloads[0]}{qw(namespace stname funcname name)};
        if ($namespace) {
            print qq/IMGUILUA_FUNCTION(${namespace}::$funcname)\n/;
        }
        else {
            print qq/IMGUILUA_METHOD(${stname}::$funcname)\n/;
        }
        print qq/static int $name(lua_State *L)\n/;
        print qq/{\n/;
        print indent(4, $resolution->(map { $_->{overload_name} } @overloads));
        print qq/}\n\n/;
        return $name;
    }
    else {
        warn "No overload resolution for:\n",
             join '', map { "    '$_',\n" } @signatures;
        return undef;
    }
}

sub generate_function($title, $defs) {
    print "// $title\n\n";

    my @overloads = sort { $a->{signature} cmp $b->{signature} }
                    map { translate_function($_) } @$defs;

    my $name;
    if (@overloads == 1) {
        $name = write_function($overloads[0], 0);
    }
    elsif (@overloads > 1) {
        write_function($_, 1) for @overloads;
        $name = generate_overload_resolution(@overloads);
    }

    print "\n";
    return $name;
}


my %struct_field_override = (
    'ImGuiLuaBoolRef.value' => '*self',
    'ImGuiLuaIntRef.value' => '*self',
    'ImGuiLuaFloatRef.value' => '*self',
    'ImGuiLuaStringRef.value' => '*self',
);

my %struct_index_override = (
    'ImGuiLuaStringRef.value' => qq/lua_getiuservalue(L, 1, 1);\n/
                               . qq/return 1;\n/,
);

my %struct_newindex_override = (
    'ImGuiLuaStringRef.value' => qq/luaL_checkstring(L, 3);\n/
                               . qq/lua_pushvalue(L, 3);\n/
                               . qq/lua_setiuservalue(L, 1, 1);\n/
                               . qq/return 0;\n/,
);

sub generate_struct_index($struct, $field) {
    my $type = $field->{type};
    my $name = $field->{name};

    my $actual_type = $type_aliases{$type} // $type;
    my $ret = $function_returns{$actual_type};
    if (!$ret) {
        print qq(    // $type $name: Unknown return type '$actual_type'\n\n);
        return;
    }

    if (defined $field->{size}) {
        print qq(    // $type $name: arrays not supported\n\n);
        return;
    }

    my $len = length $name;
    print qq/    if (len == $len && memcmp(key, "$name", $len) == 0) {\n/;

    my $override_key = "$struct.$name";
    my $index_override = $struct_index_override{$override_key};
    if ($index_override) {
        print indent(8, $index_override);
    }
    else {
        my $access = $struct_field_override{$override_key} || "self->$name";
        print indent(8, $ret->{assign}->($access));
        print indent(8, $ret->{out}->());
        print qq/        return $ret->{retcount};\n/;
    }

    print qq/    }\n\n/;
}

sub generate_struct_newindex($struct, $field) {
    my $type = $field->{type};
    my $name = $field->{name};

    my $actual_type = $type_aliases{$type} // $type;
    my $arg = $function_args{$actual_type};
    if (!$arg) {
        print qq(    // $type $name: Unknown argument type '$actual_type'\n\n);
        return;
    }

    if (defined $field->{size}) {
        print qq(    // $type $name: arrays not supported\n\n);
        return;
    }

    my $len = length $name;
    print qq/    if (len == $len && memcmp(key, "$name", $len) == 0) {\n/;

    my $override_key = "$struct.$name";
    my $newindex_override = $struct_newindex_override{$override_key};
    if ($newindex_override) {
        print indent(8, $newindex_override);
    }
    else {
        print indent(8, $arg->{declare}->({name => 'ARGVAL'}));
        print indent(8, $arg->{in}->({name => 'ARGVAL', index => 3}));
        my $access = $struct_field_override{"$struct.$name"} || "self->$name";
        print qq/        $access = ARGVAL;\n/;
        print qq/        return 0;\n/;
    }

    print qq/    }\n\n/;
}

sub generate_struct($grouped_definitions, $name, $fields) {
    my $type = "$name*";
    my $actual_type = $type_aliases{$type} // $type;
    my $arg = $function_args{$actual_type};
    if (!$arg) {
        print qq(// struct $name: Unknown argument type '$actual_type' ($type)\n\n);
        return;
    }

    print qq/static int ImGuiLua_Index$name(lua_State *L)\n/;
    print qq/{\n/;
    print indent(4, $arg->{declare}->({name => 'self'}));
    print indent(4, $arg->{in}->({name => 'self', index => 1}));
    print qq/    size_t len;\n/;
    print qq/    const char *key = luaL_checklstring(L, 2, &len);\n\n/;
    for my $field (@$fields) {
        generate_struct_index($name, $field);
    }
    print qq/    luaL_getmetafield(L, 1, key);\n/;
    print qq/    return 1;\n/;
    print qq/}\n\n/;

    print qq/static int ImGuiLua_Newindex$name(lua_State *L)\n/;
    print qq/{\n/;
    print indent(4, $arg->{declare}->({name => 'self'}));
    print indent(4, $arg->{in}->({name => 'self', index => 1}));
    print qq/    size_t len;\n/;
    print qq/    const char *key = luaL_checklstring(L, 2, &len);\n\n/;
    for my $field (@$fields) {
        generate_struct_newindex($name, $field);
    }
    print qq/    return luaL_error(L, "$name has no field '%s'", key);\n/;
    print qq/}\n\n/;

    my @methods;
    for my $defs (grep { $_->[0]{stname} eq $name } @$grouped_definitions) {
        my $title  = make_function_title($defs->[0]);
        if ($exclusions{$title}) {
            print "// $title is excluded\n\n\n";
        }
        elsif (!$defs->[0]{constructor} && !$defs->[0]{destructor}) {
            my $name = generate_function($title, $defs);
            if ($name)  {
                push @methods, {
                    c_name   => $name,
                    lua_name => $defs->[0]{funcname},
                };
            }
        }
    }

    print qq/IMGUILUA_STRUCT($name)\n/;
    print qq/static void ImGuiLua_RegisterStruct$name(lua_State *L)\n/;
    print qq/{\n/;
    print qq/    luaL_newmetatable(L, "$name");\n/;
    print qq/    lua_pushcfunction(L, ImGuiLua_Index$name);\n/;
    print qq/    lua_setfield(L, -2, "__index");\n/;
    print qq/    lua_pushcfunction(L, ImGuiLua_Newindex$name);\n/;
    print qq/    lua_setfield(L, -2, "__newindex");\n/;
    for my $method (@methods) {
        print qq/    lua_pushcfunction(L, $method->{c_name});\n/;
        print qq/    lua_setfield(L, -2, "$method->{lua_name}");\n/;
    }
    print qq/    lua_pop(L, 1);\n/;
    print qq/}\n\n/;
}


my $structs_and_enums = decode_json_file('structs_and_enums.json');
my $definitions = decode_json_file('definitions.json');
my %custom_structs = (
    ImGuiLuaBoolRef   => [{name => 'value', type => 'bool'}],
    ImGuiLuaIntRef    => [{name => 'value', type => 'int'}],
    ImGuiLuaFloatRef  => [{name => 'value', type => 'float'}],
    ImGuiLuaStringRef => [{name => 'value', type => 'const char*'}],
);


my $buffer;
{
    open local *STDOUT, '>', \$buffer
        or die "Can't open string buffer as stdout: $!\n";

    print read_include('header.cpp'), "\n\n";

    for my $name (sort keys %{$structs_and_enums->{enums}}) {
        generate_enum($name, $structs_and_enums->{enums}{$name});
    }
    print "\n";

    my @grouped_definitions = grouped_definitions($definitions);
    for my $name (sort keys %{$structs_and_enums->{structs}}) {
        generate_struct(\@grouped_definitions, $name, $structs_and_enums->{structs}{$name});
    }
    for my $name (sort keys %custom_structs) {
        generate_struct(\@grouped_definitions, $name, $custom_structs{$name});
    }
    print "\n";

    for my $defs (@grouped_definitions) {
        my $title = make_function_title($defs->[0]);
        if ($exclusions{$title}) {
            print "// $title is excluded\n\n\n";
        }
        elsif (!$defs->[0]{namespace}) {
            print "// $title has no namespace\n\n\n";
        }
        elsif (!$defs->[0]{stname}) {
            generate_function($title, $defs);
        }
    }

    print read_include('footer.cpp'), "\n\n";

    close *STDOUT or die "Can't close string buffer as stdout: $!\n";
}
$buffer =~ s{^//-.*\n}{}mg;
print $buffer;


my %namespace_to_functions;
while ($buffer =~ /^\h*IMGUILUA_FUNCTION\(\s*(\w+)::(\w+)\s*\)\s*static\s+int\s+(\w+)/mg) {
    my ($namespace, $funcname, $call) = ($1, $2, $3);
    push @{$namespace_to_functions{$namespace} ||= []}, {
        funcname  => $funcname,
        call      => $call,
    };
}

my @function_regs;
for my $namespace (sort keys %namespace_to_functions) {
    my $name = "ImGuiLua_Reg$namespace";
    push @function_regs, [$name, $namespace];

    print qq/static luaL_Reg $name\[] = {\n/;

    my @functions = sort { $a->{call} cmp $b->{call} }
                         @{$namespace_to_functions{$namespace}};
    for my $func (@functions) {
        print qq/    {"$func->{funcname}", $func->{call}},\n/;
    }

    print qq/    {NULL, NULL},\n/;
    print qq/};\n\n/;
}


print qq/extern "C" int ImGuiLua_InitImGui(lua_State *L)\n/;
print qq/{\n/;
print qq/    luaL_checktype(L, 1, LUA_TTABLE);\n/;
print qq/    lua_pushvalue(L, 1);\n/;

while ($buffer =~ /^\h*IMGUILUA_ENUM\(\s*(\w+)\s*\)\s*static\s*void\s*(\w+)/mg) {
    my ($table, $call) = ($1, $2);
    print qq/    ImGuiLua_GetOrCreateTable(L, "$table");\n/;
    print qq/    $call(L);\n/;
    print qq/    lua_pop(L, 1);\n/;
}

while ($buffer =~ /^\h*IMGUILUA_STRUCT\(\s*\w+\s*\)\s*static\s*void\s*(\w+)/mg) {
    print qq/    $1(L);\n/;
}

for my $reg (@function_regs) {
    my ($name, $namespace) = @$reg;
    print qq/    ImGuiLua_GetOrCreateTable(L, "$namespace");\n/;
    print qq/    luaL_setfuncs(L, $name, 0);\n/;
    print qq/    lua_pop(L, 1);\n/;
}

print qq/    return 0;\n/;
print qq/}\n/;
