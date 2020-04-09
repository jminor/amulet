win = am.window{
    title = "Sunvox Player",
    width = 1024,
    height = 1024
}
world = am.group()
-- 0,0 to 1024,1024 to match Sunvox's coordinate system
world_origin = vec2(win.width/2, win.height/2)
win.scene = am.translate(-world_origin) ^ world

border_color_inactive = vec4(.5,.5,.5,1)
border_color_hover = vec4(1,1,1,1)
border_color_active = vec4(1,1,0,1)

function win2world(pos)
    return pos + world_origin
end

function world2win(pos)
    return pos - world_origin
end

songs = {
    "/Users/jminor/git/sunvox/examples/simple_examples/dubstep.sunvox",
    "/Users/jminor/git/sunvox/examples/simple_examples/techno.sunvox",
    "/Users/jminor/git/sunvox/examples/simple_examples/analog_gen.sunvox",

    "/Users/jminor/git/sunvox/examples/sunvox_compo/Spline - Endless August.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/Sirmooge - Phalanx.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/manganoid - Dash of Wind.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/OceanicDegree - Dreaming.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/KnyazIvan - Cold Summer.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/Logickin Lambda - Soar Beyond.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/Twistboy - Good Times.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/KnyazIvan - Left Double Quotation Mark.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/echo2 - unreal love 3.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/Trackerbound - its over now.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/Pavel Ti - Boomer.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/KnyazIvan - fatal precession.sunvox",
    "/Users/jminor/git/sunvox/examples/sunvox_compo/Trackerbound - make a wish.sunvox",
}
song_index = 0

sunvox = am.sunvox()
sunvox_slot = 0
world:action("sunvox", am.play(sunvox, false, 1))
world:action("keyboard", function()
    if win:key_pressed("right") then
        next_song(1)
    end
    if win:key_pressed("left") then
        next_song(-1)
    end
end)

shader_program = nil

function main()
    shader_program = create_shader_program()
    next_song(1)

end

function next_song(offset)
    -- TODO: we could stop the old song, and then start the new one on a different slot
    -- so the old one would fade out?
    song_index = ((song_index - 1 + offset) % #songs) + 1
    load_song(song_index)
    show_song()
end

function load_song(index)
    sunvox:load(sunvox_slot, songs[index])
    sunvox:play_from_beginning(sunvox_slot)
end

function test_bit(number, bit)
    local x = number
    for i = 0, bit do
        x = math.floor(x/2)
    end
    return math.floor(x/2) ~= x/2
end

function show_song()
    world:remove_all()

    local rate = sunvox:get_sample_rate()

    local modules = {}
    for mod_num = 0, sunvox:get_number_of_modules(sunvox_slot)-1 do
        local flags = sunvox:get_module_flags(sunvox_slot, mod_num)
        local exists = test_bit(flags, 0)
        if exists then
            table.insert(modules, mod_num)
        end
    end

    local w = 60
    local h = 40

    -- lines first
    for _,mod_num in ipairs(modules) do
        local xy = sunvox:get_module_xy(sunvox_slot, mod_num)

        local inputs = sunvox:get_module_inputs(sunvox_slot, mod_num)
        for i,input in ipairs(inputs) do
            local iflags = sunvox:get_module_flags(sunvox_slot, input)
            local iexists = test_bit(iflags, 0)
            if iexists then
                local ixy = sunvox:get_module_xy(sunvox_slot, input)
                world:append(
                    am.line(vec2(xy[1]+w/2,xy[2]+h/2), vec2(ixy[1]+w/2, ixy[2]+h/2), 1, vec4(1,1,1,1))
                )
            end
        end

    end

    -- then the modules
    for _,mod_num in ipairs(modules) do
        local name = sunvox:get_module_name(sunvox_slot, mod_num)
        local xy = sunvox:get_module_xy(sunvox_slot, mod_num)
        local rgb = sunvox:get_module_color(sunvox_slot, mod_num)
        local color = vec4(rgb[1]/255, rgb[2]/255, rgb[3]/255, 1)

        local x1 = xy[1]
        local y1 = xy[2]
        local x2 = xy[1]+w
        local y2 = xy[2]+h

        local rect = am.rect(0,0, w,h, color)
        local mask = am.rect(0,0, w,h, vec4(0,0,0,1))
        local border = am.rect(-1,-1, w+2,h+2, border_color_inactive)
        local waveform = am.group()
        local label = am.text(
            name,
            vec4(0,0,0,1),
            "left",
            "bottom"
        )

        local module = am.translate(vec2(x1,y1)) ^ am.group { border, mask, rect, waveform, label }

        local level_meter = 0
        local muted = false

        local num_samples = 1024
        local xs = am.buffer(num_samples * 4):view("float", 0, 4)
        for i = 1, num_samples do
            xs[i] = ((i-1)/num_samples) * 2 - 1
        end
        local scope_buffer = am.buffer(num_samples * 4)
        local scope_view = scope_buffer:view("float")
        local scope_audio_buffer = am.audio_buffer(scope_buffer, 1, rate)

        waveform:append(
            am.use_program(shader_program)
            ^am.bind{
                MVP = mat4(1),
                x = xs,
                y = scope_view,
                color = vec3(color)
            }
            ^am.blend("add")
            ^am.draw("line_strip")
        )

        module:action("update", function()
            local received = sunvox:get_module_scope2(sunvox_slot, mod_num, 0, scope_audio_buffer, num_samples)
            -- waveform"bind".y = scope_view

            -- local min=0
            -- local max=0
            -- for i = 1, num_samples do
            --     local v = scope_view[i]
            --     if v < min then min = v end
            --     if v > max then max = v end
            -- end
            -- print("min/max = "..min.."/"..max)

            local pos = win2world(win:mouse_position())
            if pos.x > x1 and pos.x < x2 and pos.y > y1 and pos.y < y2 then
                border.color = border_color_hover
                if win:mouse_pressed"left" then
                    border.color = border_color_active
                    if not playing then
                        playing = true
                        rect.color = vec4(1 - color.rgb, 1)
                        next_song(1)
                        local slot = sunvox_slot
                        rect:action("play", function()
                            if sunvox:end_of_song(slot) then
                                rect.color = color
                                playing = false
                                return true
                            else
                                local level = sunvox:get_current_signal_level(slot) / 255
                                if level > level_meter then
                                    level_meter = level
                                else
                                    level_meter = level_meter - 0.05
                                    if level_meter < 0 then level_meter = 0 end
                                end
                                rect.x2 = w * level_meter
                                rect.color = vec4(
                                    rect.color.rgb,
                                    level_meter
                                )
                            end
                        end)
                    else
                        rect.color = color
                        playing = false
                        rect:cancel("play")
                    end
                end
            else
                border.color = border_color_inactive
            end
        end)
        world:append(module)
    end
end

function create_shader_program()
    local vshader = [[
        precision mediump float;
        attribute float x;
        attribute float y;
        uniform mat4 MVP;
        void main() {
            gl_Position = MVP * vec4(x, 0.5*y/32768.0, 0, 1);
        }
    ]]
    local fshader = [[
        precision mediump float;
        uniform vec3 color;
        void main() {
            gl_FragColor = vec4(color, 1.0);
        }
    ]]
    return am.program(vshader, fshader)
end

main()
