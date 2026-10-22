def floats2str(_v):
    return ','. join([f"{v:4f}" for v in _v])

def str2floats(_s):
    if _s == "":
        return []
    else:
        return [float(s) for s in _s.split(',')]
    
def ints2str(_v):
    return ','. join([f"{v}" for v in _v])

def str2ints(_s):
    if _s == "":
        return []
    else:
        return [int(s) for s in _s.split(',')]