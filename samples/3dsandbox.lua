local am = amulet
local win = am.window{
    title = "3D Scene",
    width = 1200,
    height = 800,
    resizable = true,
    --lock_pointer = true,
    depth_buffer = true,
}

local turn_speed = 2
local walk_speed = 5
local strafe_speed = 5
local floor_y = -1
local near_clip = 0.1
local far_clip = 100

local objects
local shader
local camera
local shader

local main_action
local init_shader
local create_floor
local load_model
local load_image

local texture_cache = {}

local
function init()
    init_shader()

    objects = am.group()
    objects:append(create_floor())
    local torus = load_model("torus.obj")
        :rotate("MV", quat(0, vec3(0, 1, 0))):action(function(node)
            node.rotation = quat(am.frame_time, vec3(0, 1, 0))
        end)
        :rotate("MV", quat(0, vec3(1, 0, 0))):action(function(node)
            node.rotation = quat(am.frame_time * 0.3, vec3(1, 0, 0))
        end)
        :rotate("MV", quat(0, vec3(0, 0, 1))):action(function(node)
            node.rotation = quat(am.frame_time * 0.7, vec3(0, 0, 1))
        end)
        :scale("MV", vec3(0.5))
        :cull_sphere("P", "MV", 1.5)
    for i = 1, 20000 do
        objects:append(torus
            :rotate("MV", quat(
                math.random() * math.pi * 2, math.normalize(vec3(math.random(), math.random(), math.random()))))
            :translate("MV", (math.random() - 0.5) * 100, math.random() * 50, (math.random() - 0.5) * 100))
    end
    --[[
    for i = 1, 3 do
        for j = 1, 3 do
            local tree = load_image("tree.png")
                :billboard("MV", true)
                :scale("MV", vec3(i*j))
                :translate("MV", -i*6, -1, -j*6)
                --:blend("add")
                --:pass(2)
            objects:prepend(tree)
        end
    end
    ]]

    camera = objects
        :lookat("MV", vec3(0, 0, 0), vec3(0, 0, -1), vec3(0, 1, 0))

    win.root = camera
        :bind_mat4("P", math.perspective(math.rad(70), win.width/win.height, near_clip, far_clip))
        :bind_program(shader)
        :cull_face("ccw")

    win.root:action(main_action)
end

local pitch = 0

local
function update_camera()
    local yaw = -win:mouse_position().x*2
    pitch = math.clamp(pitch + win:mouse_delta().y*2, -math.pi/2+0.001, math.pi/2-0.001)
    local dir = math.euleryxz3(vec3(pitch, yaw, 0)) * vec3(0, 0, -1)
    local pos = camera.eye
    if win:key_down("w") or win:mouse_button_down("left") then
        pos = pos + dir * walk_speed * am.delta_time
    elseif win:key_down("s") then
        pos = pos - dir * walk_speed * am.delta_time
    end
    if win:key_down("a") then
        local perp = math.cross(dir, vec3(0, 1, 0))
        pos = pos - perp * strafe_speed * am.delta_time
    elseif win:key_down("d") then
        local perp = math.cross(dir, vec3(0, 1, 0))
        pos = pos + perp * strafe_speed * am.delta_time
    end
    if pos.y < floor_y + near_clip * 2 then
        pos.y = floor_y + near_clip * 2
    end
    camera.eye = pos
    camera.center = pos + dir
end

function main_action()
    if win:key_pressed("escape") then
        win:close()
        return
    elseif win:key_pressed("lalt") then
        win.lock_pointer = not win.lock_pointer
    end
    update_camera()
end

local
function load_texture(name, swrap, twrap, minfilter, magfilter)
    if texture_cache[name] then
        return texture_cache[name]
    end
    swrap = swrap or "repeat"
    twrap = twrap or "repeat"
    minfilter = minfilter or "linear_mipmap_linear"
    magfilter = magfilter or "linear"
    local img = am.decode_png(am.read_buffer(name))
    local texture = am.texture2d{
        buffer = img.buffer,
        width = img.width,
        height = img.height,
        swrap = swrap,
        twrap = twrap,
        minfilter = minfilter,
        magfilter = magfilter,
    }
    texture_cache[name] = texture
    return texture
end

function create_floor()
    local r = 100
    local s = 100
    local y = floor_y
    return am.draw_elements(am.ushort_elem_array{1, 2, 3, 2, 4, 3})
        :bind_array("pos", am.vec3_array{
            -r, y, r,
            r, y, r,
            -r, y, -r,
            r, y, -r})
        :bind_array("uv", am.vec2_array{
            0, 0,
            s, 0,
            0, s,
            s, s})
        :bind_array("color", am.vec3_array{
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1})
        :bind_sampler2d("tex", load_texture("floor_tile.png"))
end

function init_shader() 
    local vshader = [[
        precision highp float;
        uniform mat4 MV;
        uniform mat4 P;
        attribute vec3 pos;
        attribute vec2 uv;
        attribute vec3 color;
        varying vec2 v_uv;
        varying vec3 v_color;
        void main() {
            v_uv = uv;
            v_color = color;
            gl_Position = P * MV * vec4(pos, 1);
        }
    ]]
    local fshader = [[
        precision mediump float;
        uniform sampler2D tex;
        varying vec2 v_uv;
        varying vec3 v_color;
        void main() {
            gl_FragColor = vec4(v_color, 1.0) * texture2D(tex, v_uv);
        }
    ]]
    shader = am.program(vshader, fshader)
end

function load_model(name)
    local buf, stride, normals_offset, tex_offset = am.load_obj(name)
    local verts = buf:view("vec3", 0, stride)
    local uvs = buf:view("vec2", 0, stride)
    local colors = buf:view("vec3", normals_offset, stride)
    return am.draw_arrays("triangles")
        :bind_array("pos", verts)
        :bind_array("uv", uvs)
        :bind_array("color", colors)
        :bind_sampler2d("tex", load_texture("gradient.png"))
end

function load_image(name)
    return am.draw_elements(am.ushort_elem_array{1, 2, 3, 2, 4, 3})
        :bind_array("pos", am.vec3_array{
            -1, 0, 0,
            1, 0, 0,
            -1, 2, 0,
            1, 2, 0})
        :bind_array("uv", am.vec2_array{
            0, 0,
            1, 0,
            0, 1,
            1, 1})
        :bind_array("color", am.vec3_array{
            1, 1, 1,
            1, 1, 1,
            1, 1, 1,
            1, 1, 1})
        :bind_sampler2d("tex", load_texture(name, "mirrored_repeat", "mirrored_repeat"))
end


init()
