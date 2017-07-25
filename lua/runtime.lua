--[[
-- Default VCL runtime script written in pure Lua5.1
-- For performance concern , user needs to provide the VCL.runtime inside of host
-- language instead of directly using it. However this may be good enough to handle
-- most of the cases that we have
]]--

-- Based on RFC BNF
local _valid_http_header_char_set = "[!#$%%&'*+%-.^_|~a-zA-Z0-9]"

-- shortcuts and also save us shit to do dictionary lookup
local _join = function(...)
  return table.concat(arg,"")
end

return {
  attr = {
    -- Get attribute named of "b" from object "a"
    get = function(a,b)
      local ta = type(a)
      local tb = type(b)

      assert( ta == "string" , "attribute value for object must be string type but got "..ta )
      assert( tb == "string" , "attribute key must be string type but got "..tb )

      local s , e = string.find(a,b.."=")
      if s == nil then
        error(_join("value",a,"doesn't have attribute",b))
      else
        -- to avoid iterate each single character , the best way to do is
        -- doing another string.find to find the end of the key value pair
        local semicolon = string.find(a,";",e+1)
        if semicolon == nil then
          semicolon = #a
        else
          semicolon = semicolon - 1
        end
        return string.sub(a,e+1,semicolon)
      end
    end,

    -- Set attribute named "b" into object "a" with value c
    set = function(a,b,c)
      local ta = type(a)
      local tb = type(b)
      local tc = type(c)

      assert( ta == "string" , "attribute key must be string type but got " .. ta)
      assert( tb == "string" , "attribute value for object must be string type but got ".. tb)
      assert( tc == "string" , "attribute's setting value must be string type but got ".. tc)
      local _ , ret = string.gsub(a,_join(b,"=",_valid_http_header_char_set,"*"),
                                    _join(b,"=",c))
      if ret == 0 then
        error(_join("value",a," doesn't have attribute ",b))
      end
    end,

    -- Unset attribute named "b" into object "a" with value c
    unset = function(a,b)
      return _export.set(a,b,"")
    end,

    -- Aggregator operator doesn't support attribute type currently.
    -- It may be useful for future if user want to hook the vcl.runtime
    self_add = function(a,b,c)
      error("attribute doesn't support += operator")
    end,

    self_sub = function(a,b,c)
      error("attribute doesn't support -= operator")
    end,

    self_mul = function(a,b,c)
      error("attribute doesn't support *= operator")
    end,

    self_div = function(a,b,c)
      error("attribute doesn't support /= operator")
    end,

    self_mod = function(a,b,c)
      error("attribute doesn't support %= operator")
    end
  },

  -- Unset property of object a with property name b
  unset_prop = function(a,b)
    local tb = type(b)
    assert( tb == "string" , "property name must be string type but got type "..tb )
    a[b] = nil
  end,

  -- Size object
  new_size = function(b,kb,mb,gb)
    return {
      bytes = b,
      kilobytes = kb,
      megabytes = mb,
      gigabytes = gb,

      __vcl_builtin_Type = "size"
    }
  end,

  -- Duration object
  new_duration= function(ms,s,min,hour)
    return {
      hour = hour,
      minute = min,
      second = s,
      millisecond = ms,
      __vcl_builtin_Type = "duration"
    }
  end,

  -- We need to support PCRE or just default to use native Lua's pattern
  -- matching. Anyway these 2 APIs exposes matching function for the underlying
  -- generated code to run
  match = function(a,b)
    local ta = type(a)
    local tb = type(b)
    assert(ta == "string","operator ~ only works on string type but got type "..ta)
    assert(tb == "string","pattern in ~ expression must be string type but got type "..tb)
    return string.match(a,b) ~= nil
  end,

  not_match = function(a,b)
    local ta = type(a)
    local tb = type(b)
    assert(ta == "string","operator !~ only works on string type but got type "..ta)
    assert(tb == "string","pattern in !~ expression must be string type but got type "..tb)
    return string.match(a,b) == nil
  end,

  -- Extension exposed. If we allow any extension, then we need to wrap it in the following
  -- table/namespace. Read the section of extension for more information
  extension = {}
}
