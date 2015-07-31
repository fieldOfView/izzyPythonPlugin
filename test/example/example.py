"""Example file to test izzyPythonPlugin.
In the PythonPlugin actor in Isadora, specify the following:
    path: the absolute path to the test folder, properly escaped (eg 'C:\\Users\\me\\Desktop\\izzyPythonPlugin\\test')
    module: name of the module, without .py (ie 'example')
    function: name of the function (ie 'test1' or 'test2') 
"""

def test1(arg1_int, arg2='default',arg3=12,arg4=23.5, arg5=True, arg6=None, arg7=(0,1), *args, **kwargs):
    """This function is declared in the module."""
    local_variable = str(arg1_int) + arg2
    return local_variable

def test2(string='', times=1):
    result = ""
    for i in range(0,times):
        result += string
    return result