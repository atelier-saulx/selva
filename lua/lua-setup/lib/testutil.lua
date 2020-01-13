local add = function (a, b)
    return a + b
end

local test_add_5_and_7_eql_12 = function()
    assert(add(5, 7) == 12)
end

return add