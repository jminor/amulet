local am = amulet

for i, mt in pairs(_metatable_registry) do
    mt._children = {}
end

for i, mt in pairs(_metatable_registry) do
    while mt._parent_mt do
        local parent = mt._parent_mt
        table.insert(parent._children, mt)
        mt = parent
    end
end

local
function set_mt_field_2(mt, field, val)
    for _, child in ipairs(mt._children) do
        if not rawget(child, field) then
            rawset(child, field, val)
        end
        set_mt_field_2(child, field, val)
    end
end

local
function set_mt_field(mt, field, val)
    rawset(mt, field, val)
    set_mt_field_2(mt, field, val)
end

local mtmt = {
    __newindex = set_mt_field,
}
for i, mt in pairs(_metatable_registry) do
    setmetatable(mt, mtmt)
end

local pre_frame_funcs = {}
local post_frame_funcs = {}

function am._pre_frame(dt, curr_time) 
    for i = 1, #pre_frame_funcs do
        pre_frame_funcs[i](dt, curr_time)
    end
end

function am._post_frame(dt, curr_time) 
    for i = 1, #post_frame_funcs do
        post_frame_funcs[i](dt, curr_time)
    end
end

function am._register_pre_frame_func(f)
    table.insert(pre_frame_funcs, f)
end

function am._register_post_frame_func(f)
    table.insert(post_frame_funcs, f)
end
