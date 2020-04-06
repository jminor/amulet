win = am.window{title = "Sounds"}

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

group = am.group()

sunvox = am.sunvox()
group:action("sunvox", am.play(sunvox, false, 1))
sunvox_slot = 0

for i = 0, 11 do
    for j = 0, 3 do
        local x1 = 640 / 12 * i - 320
        local y1 = 480 / 4 * j - 240
        local x2 = x1 + 640 / 12
        local y2 = y1 + 480 / 4
        local color = vec4(1-i/12, i/12, j/5 + 0.2, 1)
        local rect = am.rect(x1, y1, x2, y2, color)
        local level_meter = 0
        local playing = false
        rect:action("update", function()
            local pos = win:mouse_position()
            if pos.x > x1 and pos.x < x2 and pos.y > y1 and pos.y < y2 then
                if win:mouse_pressed"left" then
                    if not playing then
                        playing = true
                        local pitch = 2 ^ (i / 12 - 3 + j)
                        rect.color = vec4(1 - color.rgb, 1)
                        local slot = sunvox_slot
                        sunvox_slot = (sunvox_slot + 1) % 4
                        sunvox:play_file(songs[i+1], slot, 0)
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
                                rect.y2 = y1 + (y2-y1) * level_meter
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
            end
        end)
        group:append(rect)
    end
end

win.scene = group
