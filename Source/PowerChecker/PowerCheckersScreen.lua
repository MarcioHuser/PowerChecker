-- To use this script, you will need:
--	- A "Computer Case"
--  - A "CPU"
--  - A "GPU"
--	- Some "RAM"
--  - A large screen: name it "MainScreen"
--  - Connect network cables to all the checkers you want to monitor
--  - Name each one as "Checker (something)" (they must have the word "Checker", somewhere)
-- If you need more functionality (like a button that you just press to refresh the screen), it is up to you to improve as you want

spareLimit = 200

gpu = computer.getGPUs()[1]
screen = component.proxy(component.findComponent("MainScreen"))[1]

gpu:bindScreen(screen)

-- columns = 60, lines = 22. Make your screen have the same number of columns, and ajust the lines accordingly to the size of the screen you made
gpu:setSize(60, 22)

w,h=gpu:getSize()

function resetBG()
    gpu:setBackground(0,0,0,1)
end

function roungDigits(value, digits)
    m = 10 ^ (digits or 4)
    return math.floor(value * m + 0.5) / m
end

function setBlueBG()
    gpu:setBackground(0,0,0.15,1)
end

function setGreenBG()
    gpu:setBackground(0,0.15,0,1)
end

function setRedBG()
    gpu:setBackground(0.15,0,0,1)
end

function setYellowBG()
    gpu:setBackground(0.25,0.15,0,1)
end

function updateChecker()
    checkers = component.proxy(component.findComponent("Power Checker"))
    
    table.sort(checkers, function(a,b) return a.nick < b.nick end)

	line = 1

	resetBG()

	gpu:fill(0,0,w,h,' ')
	gpu:flush()

	gpu:setText(0,0,"Name")
	gpu:setText(w-26,0,"Cap.")
	gpu:setText(w-16,0,"Cons.")
	gpu:setText(w-7,0,"Max.")

	for _,checker in pairs(checkers) do
		gpu:setText(0,line, "- " .. checker.nick)

       for _,m in pairs(checker:getMembers()) do
             print(m)
       end

		checker:updateScreen()
		
		maximumConsumption = checker:maximumConsumption()
		capacity = checker:getPowerConnectors()[1]:getCircuit().capacity
		consumption = checker:getPowerConnectors()[1]:getCircuit().consumption
		spare = capacity - maximumConsumption

		if spare < 0 then
			setRedBG()
		elseif spare < spareLimit then
			setYellowBG()
		end
		
		gpu:fill(w-30,line,30,1,' ')
		
		gpu:setText(w-30,line, string.format("%9g", roungDigits(capacity, 0)))
		gpu:setText(w-20,line, string.format("%9g", roungDigits(consumption, 0)))
		gpu:setText(w-10,line, string.format("%9g", roungDigits(maximumConsumption, 0)))

		gpu:setBackground(0,0,0,1)

		line = line + 1
	end

	gpu:flush()
end

updateChecker()
