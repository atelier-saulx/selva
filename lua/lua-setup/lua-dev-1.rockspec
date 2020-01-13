package = "lua"
version = "dev-1"
source = {
   url = "git+ssh://git@github.com/atelier-saulx/selva-client"
}
description = {
   homepage = "*** please enter a project homepage ***",
   license = "*** please specify a license ***"
}
dependencies = {
   "testy"
}
build = {
   type = "builtin",
   modules = {
      ["lib.testutil"] = "lib/testutil.lua"
   }
}
