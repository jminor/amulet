local win = am.window{
    title = "speech",
    width = 320, height = 240}

math.randomseed(os.time())

local colors = {
    vec4(1, 1, 0, 1),
    vec4(1, 0, 0, 1),
    vec4(0.8, 0.8, 0.8, 1),
    vec4(0, 1, 0, 1),
    vec4(0.0, 0.5, 1, 1),
    vec4(0.5, 0.3, 0.9, 1),
    vec4(0, 0.5, 0, 1),
    vec4(0, 1, 1, 1),
    vec4(1, 0, 1, 1),
    vec4(1, 0.5, 0, 1),
}

local black = vec4(0, 0, 0, 1)
local white = vec4(1)

local add_to_list

local
function button(x, y, label)
    local x1, y1, x2, y2 = -100, -6, 100, 8
    local node = am.translate(x, y)
        ^ {
            am.rect(x1, y1, x2, y2, colors[math.random(1000000) % #colors + 1]),
            am.text(label, black, "right")
        }
    node:action(function()
        if win:mouse_pressed"left" then
            local pos = win:mouse_position()
            local bpos = node.position2d
            if pos.x - bpos.x > x1 and pos.x - bpos.x < x2 and
                pos.y - bpos.y > y1 and pos.y - bpos.y < y2 
            then
                print("text: "..label.." -> "..am.text_to_phonemes(label))
                buf = am.speech_synth(label)
                node:action(am.play(buf))
                -- print("seed: "..seed)
                -- if gen then
                --     add_to_list(n, seed, buf)
                -- end
            end
        end
    end)
    return node
end

local list = am.group()

function add_to_list(n, seed, buf)
    if list.num_children >= 10 then
        for i, child in list:child_pairs() do
            child.position2d = vec2(120, 100 - (i-2) * 20)
        end
        list:remove(list:child(1))
        list:append(button(120, -80, seed, false, n, seed, buf))
    else
        list:append(button(120, 100 - list.num_children * 20, seed, false, n, seed, buf))
    end
end

win.scene = am.group{
    button(-30, 100, "Hello"),
    button(-30,  80, "This is a test"),
    button(-30,  60, "1 2 3 4 5"),
    button(-30,  40, "12345"),
    button(-30,  20, "Yes. Yes, Yes"),
    button(-30,   0, "Fundamental"),
    button(-30, -20, "Intruder alert"),
    button(-30, -40, "@ # $ % ^ & *"),
    button(-30, -60, "Does not compute"),
    button(-30, -80, "Good bye"),
    list,
}

win.scene:action(function()
    if win:key_pressed"escape" then
        win:close()
    end
end)
