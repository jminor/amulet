local am = amulet

local vshader = [[
    attribute float x;
    attribute float y;
    uniform mat4 MVP;
    void main() {
        gl_Position = MVP * vec4(x, y, 0, 1);
    }
]]

local fshader = [[
    precision mediump float;
    uniform vec4 tint;
    void main() {
        vec2 uv = gl_FragCoord.xy / 50.0;
        float r = sin(uv.x) * 0.4 + 0.6;
        float g = cos(uv.x) * 0.4 + 0.6;
        float b = cos(uv.y + uv.x) * 0.4 + 0.6;
        gl_FragColor = vec4( r, g, b, 1.0 ) * tint;
    }
]]

local win = am.create_window({title = "test1"})

local prog = am.program(vshader, fshader)

local buf = am.buffer(4 * 6)
local xview = buf:view("float", 0, 8)
local yview = buf:view("float", 4, 8)
xview[1] = -0.5
xview[2] = 0
xview[3] = 0.5
yview[1] = -0.4
yview[2] = 0.6
yview[3] = -0.4

local MVP = math.mat4(1)
local base = am.draw_arrays()
    :bind_array("x", xview)
    :bind_array("y", yview)

local group = am.empty()

local seeds = {}
for i = 1, 1000 do
    table.insert(seeds, math.random())
end

for i, seed in ipairs(seeds) do
    local node = base
        :scale("MVP"):alias("size")
        :translate("MVP"):alias("position")
        :bind_vec4("tint", math.random(), math.random(), math.random(), 1)
    group:append(node)
end

local top = group:bind_mat4("MVP", MVP):program(prog)

local t = 0
top:action(function()
    for i, seed in ipairs(seeds) do
        local node = group:child(i)
        local r = math.cos(t * seed * 2)
        local x = math.sin((t + seed * 6) * seed) * r 
        local y = math.cos(t - i * seed) * r
        node.position.x = x
        node.position.y = y
        node.size.xy = math.sin(t * seed * 4 + i) * 0.15 + 0.2
    end
    t = t + 1/60
    return 0
end)

win.root = top
