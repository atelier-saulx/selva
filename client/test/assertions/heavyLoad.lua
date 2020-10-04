local function logit(msg)
  redis.pcall("PUBLISH", "log", msg)
end
for i = 1,20000000 do 
  logit(i) 
end
return "x"