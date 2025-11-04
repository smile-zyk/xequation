safe_globals = {"__builtins__": {}}
local_dict = {}

exec("import math\na=math.cos(1)", safe_globals, local_dict) 