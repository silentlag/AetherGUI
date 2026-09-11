function aether_info()
	return {
		name = "Example Moving Average (Lua)",
		description = "Averages the last N pen positions. Reference Aether Lua plugin - no compiler needed."
	}
end

function aether_options()
	return {
		{
			key = "window",
			label = "Window",
			type = "slider",
			min = 2,
			max = 20,
			default = 5,
			format = "%.0f",
			description = "Number of positions to average"
		}
	}
end

function aether_create()
	return {
		window = 5,
		xs = {},
		ys = {},
		ps = {}
	}
end

function aether_set(state, key, value)
	if key == "window" then
		state.window = math.floor(math.max(2, math.min(20, value)))
		return true
	end
	return false
end

function aether_reset(state, point)
	state.xs = {}
	state.ys = {}
	state.ps = {}
	if point then
		table.insert(state.xs, point.x)
		table.insert(state.ys, point.y)
		table.insert(state.ps, point.pressure)
	end
end

function aether_process(state, point)
	table.insert(state.xs, point.x)
	table.insert(state.ys, point.y)
	table.insert(state.ps, point.pressure)

	while #state.xs > state.window do
		table.remove(state.xs, 1)
		table.remove(state.ys, 1)
		table.remove(state.ps, 1)
	end

	local sx, sy, sp = 0.0, 0.0, 0.0
	for i = 1, #state.xs do
		sx = sx + state.xs[i]
		sy = sy + state.ys[i]
		sp = sp + state.ps[i]
	end

	local n = #state.xs
	if n > 0 then
		point.x = sx / n
		point.y = sy / n
		point.pressure = sp / n
	end

	return point
end
