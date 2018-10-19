require "bootstrap-tests"

local effil = effil

test.thread_interrupt.tear_down = default_tear_down

local function interruption_test(worker)
    local state = effil.table({ stop = false })

    local ctx = effil.thread(worker)
    ctx.step = 0
    local thr = ctx(state)

    effil.sleep(1) -- let thread starts

    local start_time = os.time()
    thr:cancel(10)
    test.equal(thr:status(), "canceled")
    test.almost_equal(os.time(), start_time, 1)
    state.stop = true
end

test.thread_interrupt.thread_wait = function()
    interruption_test(function(state)
        effil.thread(function()
            while not state.stop do end
        end)():wait()
    end)
end

test.thread_interrupt.thread_get = function()
    interruption_test(function(state)
        local ctx = effil.thread(function()
            while not state.stop do end
        end)
        ctx.step = 0
        ctx():get()
    end)
end

test.thread_interrupt.thread_cancel = function()
    interruption_test(function(state)
        local ctx = effil.thread(function()
            while not state.stop do end
        end)
        ctx.step = 0
        ctx():cancel()
    end)
end

test.thread_interrupt.thread_pause = function()
    interruption_test(function(state)
        local ctx = effil.thread(function()
            while not state.stop do end
        end)
        ctx.step = 0
        ctx():pause()
    end)
end

test.thread_interrupt.sleep = function()
    interruption_test(function()
        effil.sleep(20)
    end)
end

test.thread_interrupt.yield = function()
    interruption_test(function()
        while true do
            effil.yield()
        end
    end)
end

test.thread_interrupt.channel_pop = function()
    interruption_test(function()
        effil.channel():pop()
    end)
end
