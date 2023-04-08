-- Copyright (c) 2022 askmeaboutloom
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in
-- all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.
local Translator <const> = require("class")("Translator")

function Translator:init(language)
    self.wanted_language = language
    self.categories = require("translations." .. language)
    self.cache = {}
end

function Translator:get_from_value(value, args)
    if type(value) == "string" then
        return value
    else
        return value(args)
    end
end

function Translator:get_in_category(category, key, args)
    local value = category[key]
    if value then
        return self:get_from_value(value, args)
    else
        error("Missing text")
    end
end

function Translator:get(category_key, key, args)
    local category = self.categories[category_key]
    if category then
        return self:get_in_category(category, key, args)
    else
        error("Missing category")
    end
end

function Translator:get_or_fallback(category_key, key, args)
    local ok, result = pcall(self.get, self, category_key, key, args)
    if ok then
        return result or key
    else
        warn(string.format("Translator category '%s', text '%s': %s",
                           category_key, key, result))
        return key
    end
end

function Translator:get_with_cache(category_key, args)
    local key = args.key
    local suffix = ""
    local id_start = string.find(key, "##", 1, true)
    if id_start then
        if id_start == 1 then
            suffix = key
            key = string.sub(key, id_start + 2)
        else
            suffix = string.sub(key, id_start)
            key = string.sub(key, 1, id_start - 1)
        end
    end

    local result
    if args.cache_key == false then
        result = self:get_or_fallback(category_key, key, args)
    else
        local cache_key = category_key .. "." .. (args.cache_key or key)
        result = self.cache[cache_key]
        if not result then
            result = self:get_or_fallback(category_key, key, args)
            self.cache[cache_key] = result
        end
    end
    return result .. suffix
end

function Translator:for_category(category_key)
    return function (args_or_key)
        local args = type(args_or_key) == "string"
                 and {key = args_or_key}
                  or args_or_key
        return self:get_with_cache(category_key, args)
    end
end


local Translations <const> = {
    translator = Translator:new("en_us"),
}

function Translations.for_category(class, category_key)
    return class.translator:for_category(category_key)
end

return Translations
