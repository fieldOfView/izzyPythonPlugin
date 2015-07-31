# izzyPythonPlugin

A plugin for TroikaTronix Isadora that executes a Python function and outputs the returnvalue.

## Compiling

### Windows

To compile the plugin on Windows, you need the following:
* Microsoft Visual C++ 2010 (Express Edition or Professional)
* Troikatronix Isadora
	Installed in ```C:\Program Files(x86)\Isadora```
* Troikatronix Isadora SDK
	Installed in ```C:\Program Files(x86)\IsadoraSDK```
* Apple Quicktime SDK 7.3
	Installed in ```C:\Program Files(x86)\Quicktime SDK```
* Python 2.7.x or 3.4.x

You must install a 32-bit version of Python, even if your system is capable of running the 64-bit version. Isadora is 32-bit, and so are plugins written for it. A 32-bit application can not link against a 64-bit version of Python. 

The Visual C++ project is set up in a way to expect Python installed in ```C:\Python27```. If you have Python installed in a different location, or if you use a different version (such as Python 3.4), you can set an environment variable named ```PYTHON``` pointing to the location of the version of Python you want to use.

The project was written for and tested with Python 2.7.10, but should also work with Python 3.4. Unfortunately the standard releases of Python do not come with the required libs to compile a debug-version of the plugin. This can be worked around by making a copy of the file ```python27.lib``` resp ```python34.lib``` in the folder ```%PYTHON%\libs```, named ```python27_d.lib``` resp ```python34_d.lib```. You only have to do this if you want to create a debug-build of the plugin.

Once all the dependencies have been met, the project should compile and copy the plugin into the Isadora plugin folder.

### OSX

Patches are welcome.

## Usage

The pluging is named ```PythonPlugin``` in Isadora. Once added to a scene, you can specify a path to a Python module, the name of the module and a name of a function within that module. The path is optional if the module is in your ```PYTHONPATH``` (ie: if you can 'import' the module from anywhere on your system). Note that on Windows, a '\' in the path must be escaped as '\\' (eg ```C:\\Users\\etc```). The module must reside in a folder with an ```__init__.py``` file, see the supplied example. The module name must be specified without the '.py' extension (eg ```example```).

With the path, modulename and functionname entered, the plugin should show that it has found the function in its first output (named ```function found```). If it doesn't, make sure the path and modulename are correct. Also check there are no syntax errors in the Python file.

Once the function has been discovered by the plugin, the ```get args``` input can be triggered. This will create input properties for the actor. The plugin tries to guess the best property type for each input:
* Arguments with a default value are set to be the type that fits with that defaultvalue (ie: Boolean, Int, Float, Str)
* Arguments without a default value are considered to be Strings, except when their name ends with '_int', '_bool' or '_float', in which case they are considered to be of those types.

Finally, with the properties populated, you can run the function by using the ```trigger``` input. If the function executes succesfully, the returnvalue of the function is output on the ```output``` property, and the ```function ran``` output is triggered. If an error occurs while executing the function, ```function ran``` is not triggered, and the error text is shown on the ```error``` output.

## Credits

The plugin is based on "found code" by Mark F. Coniglio. It has been extensively updated by Aldo Hoeben / fieldOfView.com for the HKU Maplab.

## License

This plugin is released under the MIT License