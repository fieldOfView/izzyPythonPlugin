// ===========================================================================
//	Isadora Python Plugin							©2015 Aldo Hooeben,
//													©2001 Mark F. Coniglio.
//													All rights reserved.
// ===========================================================================
//
//	Based on ExecutePythonFunction.cpp ©2001 Mark F. Coniglio.
//
//	IMPORTANT: This source code ("the software") is supplied to you in
//	consideration of your agreement to the following terms. If you do not
//	agree to the terms, do not install, use, modify or redistribute the
//	software.
//
//	Mark Coniglio (dba TroikaTronix) grants you a personal, non exclusive
//	license to use, reproduce, modify this software with and to redistribute it,
//	with or without modifications, in source and/or binary form. Except as
//	expressly stated in this license, no other rights are granted, express
//	or implied, to you by TroikaTronix.
//
//	This software is provided on an "AS IS" basis. TroikaTronix makes no
//	warranties, express or implied, including without limitation the implied
//	warranties of non-infringement, merchantability, and fitness for a
//	particular purpurse, regarding this software or its use and operation
//	alone or in combination with your products.
//
//	In no event shall TroikaTronix be liable for any special, indirect, incidental,
//	or consequential damages arising in any way out of the use, reproduction,
//	modification and/or distribution of this software.
//

// ---------------------------------------------------------------------------------
// INCLUDES
// ---------------------------------------------------------------------------------

#include "IsadoraTypes.h"
#include "IsadoraCallbacks.h"
#include "PluginDrawUtil.h"

// STANDARD INCLUDES
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#include <Python.h>

// ---------------------------------------------------------------------------------
//	Python 3 support
// ---------------------------------------------------------------------------------

#if PY_MAJOR_VERSION >= 3
#define PyString_FromString PyUnicode_FromString
#define PyString_AsString PyUnicode_AsUTF8
#define PyInt_FromLong PyLong_FromLong
#define PyInt_AsLong PyLong_AsLong
#endif

// ---------------------------------------------------------------------------------
// MacOS Specific
// ---------------------------------------------------------------------------------
#if TARGET_OS_MAC
#define EXPORT_
#endif

// ---------------------------------------------------------------------------------
// Win32  Specific
// ---------------------------------------------------------------------------------
#if TARGET_OS_WIN32

	#include <windows.h>
	
	#define EXPORT_ __declspec(dllexport)
	
	#ifdef __cplusplus
	extern "C" {
	#endif

	BOOL WINAPI DllMain ( HINSTANCE hInst, DWORD wDataSeg, LPVOID lpvReserved );

	#ifdef __cplusplus
	}
	#endif

	BOOL WINAPI DllMain (
		HINSTANCE	/* hInst */,
		DWORD		wDataSeg,
		LPVOID		/* lpvReserved */)
	{
	switch(wDataSeg) {
	
	case DLL_PROCESS_ATTACH:
		return 1;
		break;
	case DLL_PROCESS_DETACH:
		break;
		
	default:
		return 1;
		break;
	}
	return 0;
	}

#endif

// ---------------------------------------------------------------------------------
//	Exported Function Definitions
// ---------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

EXPORT_ void GetActorInfo(void* inParam, ActorInfo* outActorParams);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------------
//	FORWARD DECLARTIONS
// ---------------------------------------------------------------------------------
static void
AddArgInputProperties(	
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo);

static void
ClearArgInputProperties(	
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo);


// ---------------------------------------------------------------------------------
// GLOBAL VARIABLES
// ---------------------------------------------------------------------------------
// Declare global variables, common to all instantiations of this plugin here


// ---------------------------------------------------------------------------------
// Property struct
// ---------------------------------------------------------------------------------
// This structure is used to store property names, types and values in the
// PluginInfo struct.

struct Property {
	char*				name;		// display name of the property
	Value*				value;		// contains type and data
};

// ---------------------------------------------------------------------------------
// PluginInfo struct
// ---------------------------------------------------------------------------------
// This structure needs to contain all variables used by your plugin. Memory for
// this struct is allocated during the CreateActor function, and disposed during
// the DisposeActor function, and is private to each copy of the plugin.
//
// If your plugin needs global data, declare them as static variables within this
// file. Any static variable will be global to all instantiations of the plugin.

typedef struct {
	ActorInfo*			mActorInfoPtr;
	
	char*				mPath;
	char*				mFile;
	char*				mFunc;
	
	unsigned int		mNumArgs;
	bool				mFuncFound;

	Property**			mArgs;
} PluginInfo;

// A handy macro for casting the mActorDataPtr to PluginInfo*
#if __cplusplus
#define	GetPluginInfo_(actorDataPtr)		static_cast<PluginInfo*>((actorDataPtr)->mActorDataPtr);
#else
#define	GetPluginInfo_(actorDataPtr)		(PluginInfo*)((actorDataPtr)->mActorDataPtr);
#endif

// ---------------------------------------------------------------------------------
//	Constants
// ---------------------------------------------------------------------------------
//	Defines various constants used throughout the plugin.

// GROUP ID
// Define the group under which this plugin will be displayed in the Isadora interface.
// These are defined under "Actor Types" in IsadoraTypes.h

static const OSType	kActorClass 	= kGroupControl;

// PLUGIN IN
// Define the plugin's unique four character identifier. Contact TroikaTronix to
// obtain a unique four character code if you want to ensure that someone else
// has not developed a plugin with the same code. Note that TroikaTronix reserves
// all plugin codes that begin with an underline, an at-sign, and a pound sign
// (e.g., '_', '@', and '#'.)

static const OSType	kActorID		= FOUR_CHAR_CODE('0OtC');

// ACTOR NAME
// The name of the actor. This is the name that will be shown in the User Interface.

static const char* kActorName		= "PythonPlugin";

// PROPERTY DEFINITION STRING
// The property string. This string determines the inputs and outputs for your plugin.
// See the IsadoraCallbacks.h under the heading "PROPERTY DEFINITION STRING" for the
// meaning ofthese codes. (The IsadoraCallbacks.h header can be seen by opening up
// the IzzySDK Framework while in the Files view.)
//
// IMPORTANT: You cannot use spaces in the property name. Instead, use underscores (_)
// where you want to have a space.
//
// Note that each line ends with a carriage return (\r), and that only the last line of
// the bunch ends with a semicolon. This means that what you see below is one long
// null-terminated c-string, with the individual lines separated by carriage returns.

static const char* sPropertyDefinitionString =

// INPUT PROPERTY DEFINITIONS
//	TYPE 		PROPERTY NAME	ID		DATATYPE	DISPLAY FMT			MIN		MAX		INIT VALUE
	"INPROP		trigger			trig	bool		trig				0		1		0\r"
	"INPROP		path			path	string		text				*		*		\r"
	"INPROP		module			file	string		text				*		*		\r"
	"INPROP		function		func	string		text				*		*		\r"
	"INPROP		get_args		parm	bool		trig				0		1		0\r"

// OUTPUT PROPERTY DEFINITIONS
//	TYPE 		PROPERTY NAME	ID		DATATYPE	DISPLAY FMT			MIN		MAX		INIT VALUE
	"OUTPROP 	function_found	fnd		bool		onoff				0		1		0\r"
	"OUTPROP 	function_ran	ran		bool		trig				0		1		0\r"
	"OUTPROP	error			err		string		text				*		*		\r"
	"OUTPROP	output			out		string		text				*		*		\r";

// Property Index Constants
// Properties are referenced by a one-based index. The first input property will
// be 1, the second 2, etc. Similarly, the first output property starts at 1.
// You whould have one constant for each input and output property defined in the
// property definition string.

enum
{
	kInputTrigger = 1,
	kInputPath,
	kInputFile,
	kInputFunc,
	kInputGetArgs,
	kInputArg0,
	
	kOutputFuncFound = 1,
	kOutputTrigger,
	kOutputError,
	kOutputResult
};


// ---------------------
//	Help String
// ---------------------
// Help Strings
//
// The first help string is for the actor in general. This followed by help strings
// for all of the inputs, and then by the help strings for all of the outputs. These
// should be given in the order that they are defined in the Property Definition
// String above.
//
// In all, the total number of help strings should be (num inputs + num outputs + 1)
//
// Note that each string is followed by a comma -- it is a common mistake to forget the
// comma which results in the two strings being concatenated into one.

const char* sHelpStrings[] =
{
	"Finds a python function and performs the operation.",
	
	"Triggers execution of the function, if the function can be found.",
	
	"Specifies the directory of the python module. Can be left blank if the module is in the PYHTONPATH.",
	
	"Specifies a python module within the selected directory or within PYTHONPATH.",
	
	"Specifies a python function within the selected module.",
	
	"When triggered, inputs are added for each argument of the python function.",
	
	"Argument for the python function.",
	
	"Set to 'on' if the specified python function was found.",
	
	"Triggered when the function has succesfully executed.",
	
	"Outputs any error string returned by the python function. ",
	
	"Outputs data returned by the python function. ",
};

// ---------------------------------------------------------------------------------
//		¥ CreateActor
// ---------------------------------------------------------------------------------
// Called once, prior to the first activation of an actor in its Scene. The
// corresponding DisposeActor actor function will not be called until the file
// owning this actor is closed, or the actor is destroyed as a result of being
// cut or deleted.

static void
CreateActor(
	IsadoraParameters*	ip,	
	ActorInfo*			ioActorInfo)		// pointer to this actor's ActorInfo struct - unique to each instance of an actor
{
	// create the PluginInfo struct - initializing it to all zeroes
	PluginInfo* info = (PluginInfo*) IzzyMallocClear_(ip, sizeof(PluginInfo));
	PluginAssert_(ip, info != nil);
	
	ioActorInfo->mActorDataPtr = info;
	info->mActorInfoPtr = ioActorInfo;
	
	info->mNumArgs = 0;
	info->mFuncFound = false;
	info->mArgs = NULL;
}

// ---------------------------------------------------------------------------------
//		¥ DisposeActor
// ---------------------------------------------------------------------------------
// Called when the file owning this actor is closed, or when the actor is destroyed
// as a result of its being cut or deleted.
//
static void
DisposeActor(
	IsadoraParameters*	ip,
	ActorInfo*			ioActorInfo)		// pointer to this actor's ActorInfo struct - unique to each instance of an actor
{
	PluginInfo* info = GetPluginInfo_(ioActorInfo);
	PluginAssert_(ip, info != nil);
	
	// destruction of private member variables
	if (info->mPath != NULL)
		free(info->mPath);
	if (info->mFile != NULL)
		free(info->mFile);
	if (info->mFunc != NULL)
		free(info->mFunc);
	
	if (info->mArgs != NULL)
	{
		int i, size;
		size = info->mNumArgs;
		for (i=0; i<size; i++)
		{
			free(info->mArgs[i]->name);
			if (info->mArgs[i]->value->type == kString)
			{
				ReleaseValueString_(ip, info->mArgs[i]->value);
			}
			free(info->mArgs[i]->value);
			free(info->mArgs[i]);
		}
		free(info->mArgs);
	}
	
	// destroy the PluginInfo struct allocated with IzzyMallocClear_ the CreateActor function
	PluginAssert_(ip, ioActorInfo->mActorDataPtr != nil);
	IzzyFree_(ip, ioActorInfo->mActorDataPtr);
}

// ---------------------------------------------------------------------------------
//		¥ ActivateActor
// ---------------------------------------------------------------------------------
//	Called when the scene that owns this actor is activated or deactivated. The
//	inActivate flag will be true when the scene is activated, false when deactivated.
//
static void
ActivateActor(
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo,		// pointer to this actor's ActorInfo struct - unique to each instance of an actor
	Boolean				inActivate)			// true when actor is becoming active, false otherwise.
{
	PluginInfo* info = GetPluginInfo_(inActorInfo);
	
	// ------------------------
	// ACTIVATE
	// ------------------------
	if (inActivate)
	{
	
		// Isadora passes various messages to plugins that request them.
		// These include Mouse Moved messages, Key Down/Key Up messages,
		// Video Frame Clock messages, etc. The complete list can be found
		// in the enumeration in MessageReceiverCommon.h
		
		// You ask Isadora¨ for these messages by calling CreateMessageReceiver_
		// with a pointer to your function, and the message types you would
		// like to receive. (These are bitmapped flags, so you can combine as
		// many as you like: kWantKeyDown | kWantKeyDown for instance.)
		
	
	// ------------------------
	// DEACTIVATE
	// ------------------------
	
	}
	else
	{
	
	}
}

// ---------------------------------------------------------------------------------
//		¥ GetParameterString
// ---------------------------------------------------------------------------------
//	Returns the property definition string. Called when an instance of the actor
//	needs to be instantiated.

static const char*
GetParameterString(
	IsadoraParameters*	/* ip */,
	ActorInfo*			/* inActorInfo */)
{
	return sPropertyDefinitionString;
}

// ---------------------------------------------------------------------------------
//		¥ GetHelpString
// ---------------------------------------------------------------------------------
//	Returns the help string for a particular property. If you have a fixed number of
//	input and output properties, it is best to use the PropertyTypeAndIndexToHelpIndex_
//	function to determine the correct help string to return.

static void
GetHelpString(
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo,
	PropertyType		inPropertyType,			// kPropertyTypeInvalid when requesting help for the actor
												// or kInputProperty or kOutputProperty when requesting help for a specific property
	PropertyIndex		inPropertyIndex1,		// the one-based index of the property (when inPropertyType is not kPropertyTypeInvalid)
	char*				outParamaterString,		// receives the help string
	UInt32				inMaxCharacters)		// size of the outParamaterString buffer
{
	const char* helpstr = nil;
	// if the input the user is asking about is
	// past the end of the fixed properties, then
	// we force it to the first variable property
	if (inPropertyType == kInputProperty)
	{
		if (inPropertyIndex1 >= kInputArg0)
			inPropertyIndex1 = kInputArg0;
	}
	
	// The PropertyTypeAndIndexToHelpIndex_ converts the inPropertyType and
	// inPropertyIndex1 parameters to determine the zero-based index into
	// your list of help strings.
	UInt32 index1 = PropertyTypeAndIndexToHelpIndex_(ip, inActorInfo, inPropertyType, inPropertyIndex1);
	
	if (inPropertyType == kOutputProperty)
		index1 = kInputArg0+inPropertyIndex1;

	// get the help string
	helpstr = sHelpStrings[index1];
	
	// copy it to the output string
	strncpy(outParamaterString, helpstr, inMaxCharacters);
}

// ---------------------------------------------------------------------------------
//		¥ CreatePropertyID	[INTERRUPT SAFE]
// ---------------------------------------------------------------------------------

inline OSType
CreatePropertyID(
	IsadoraParameters*	ip,
	const char*			inRateBase,
	SInt32				inIndex)
{
	const SInt32 kOneCharMax = 26;
	const SInt32 kTwoCharMax = kOneCharMax * kOneCharMax;
	
	PluginAssert_(ip, inRateBase[0] != 0 && inRateBase[1] != 0);
	PluginAssert_(ip, inIndex >= 0 && inIndex < kTwoCharMax * 2);
	
	OSType	result = (((UInt32) inRateBase[0]) << 24) | (((UInt32) inRateBase[1]) << 16);
	
	SInt32 indexLS;
	SInt32 indexMS;
	SInt32 indexOffset;
	
	// in index is between 00 and 99
	if (inIndex >= 0 && inIndex < 100)
	{	
		indexMS = inIndex / 10;
		indexLS = inIndex % 10;
		
		result |= ( (((UInt32) (indexMS + '0')) << 8) | (((UInt32) (indexLS + '0')) << 0) );
		
		// if between 100 and 776
	}
	else if (inIndex >= 100 && inIndex < 100 + kTwoCharMax)
	{	
		indexOffset = inIndex - 100;
		PluginAssert_(ip, indexOffset >= 0 && indexOffset < kTwoCharMax);
		indexMS = indexOffset / kOneCharMax;
		indexLS = indexOffset % kOneCharMax;
		
		result |= ( (((UInt32) (indexMS + 'A')) << 8) | (((UInt32) (indexLS + 'A')) << 0) );
		
		// if between 776 and 1452
	}
	else if (inIndex >= 100 + kTwoCharMax && inIndex < 100 + kTwoCharMax * 2)
	{	
		indexOffset = inIndex - (100 + kTwoCharMax);
		PluginAssert_(ip, indexOffset >= 0 && indexOffset < kTwoCharMax);
		indexMS = indexOffset / kOneCharMax;
		indexLS = indexOffset % kOneCharMax;
		
		result |= ( (((UInt32) (indexMS + 'a')) << 8) | (((UInt32) (indexLS + 'a')) << 0) );
		
	}
	else
	{
		PluginAssert_(ip, false);
	}
	
	return result;
}

// ---------------------------------------------------------------------------------
//		 FindPythonFunc
// ---------------------------------------------------------------------------------

static void
FindPythonFunc(
	IsadoraParameters*	ip,
	PluginInfo* info )
{	
	PyObject *pName, *pModule, *pDict, *pFunc = NULL, *pInspect, *argspec_tuple, *arglist, *defaults, *defaultvalue;
	int size = 0, i;
	
	// NB: PyObjects returned by PyObject_*, PyNumber_*, PySequence_* or PyMapping_* functions must 
	// be dererefereced using Py_DECREF, PyObjects returned by PyString_*, PyTuple_* etc must not!
	// See https://docs.python.org/2/c-api/intro.html#reference-counts
	
	if (info->mArgs != NULL)
	{
		// free memory for previously created args
		size = info->mNumArgs;
		for (i=0; i<size; i++)
		{
			free(info->mArgs[i]->name);
			if (info->mArgs[i]->value->type == kString)
			{
				ReleaseValueString_(ip, info->mArgs[i]->value);
			}
			free(info->mArgs[i]->value);
			free(info->mArgs[i]);
		}
		free(info->mArgs);
		info->mArgs = NULL;
		size = 0;
	}

	info->mFuncFound = false;
	info->mNumArgs = 0;

	if (info->mFile == NULL || strlen(info->mFile) == 0 || info->mFunc == NULL || strlen(info->mFunc) == 0)
		return;

	// Initialize the python interpreter
	Py_Initialize();
	
	// Make sure we are getting the module from the correct place
	if (info->mPath != NULL && strlen(info->mPath) > 0)
	{
		char *buffer = (char*)malloc( strlen(info->mPath)+25 );
		sprintf(buffer, "sys.path.append(\"%s\")", info->mPath);

		PyRun_SimpleString("import sys");
		PyRun_SimpleString(buffer);
		free(buffer);
	}
	
	// Build the name object
	pName = PyString_FromString(info->mFile);
	if (pName != NULL)
	{	
		// Load the module object
		pModule = PyImport_Import(pName);
		if (pModule != NULL)
		{
			pDict = PyModule_GetDict(pModule);
			if (pDict != NULL)
			{
				pFunc = PyDict_GetItemString(pDict, info->mFunc);
			}
		}
	}
	
	info->mFuncFound = (pFunc != NULL && PyCallable_Check(pFunc));
	
	pName = PyString_FromString("inspect");	
	if (pName != NULL)
	{
		pInspect = PyImport_Import(pName);
		if (pInspect != NULL)
		{
			pName = PyString_FromString("getargspec");
			if (pName != NULL)
			{
				argspec_tuple = PyObject_CallMethodObjArgs(pInspect, pName, pFunc, NULL);
				if (argspec_tuple != NULL)
				{
					arglist = PyTuple_GetItem(argspec_tuple, 0);
					defaults = PyTuple_GetItem(argspec_tuple, 3);
					if (arglist != NULL && defaults != NULL)
					{
						// get the number arguments
						size = (int)PyObject_Size(arglist);
						
						int defaults_offset = (int)PyObject_Size(defaults) - size;
						
						//allocate memory for properties
						info->mArgs = (Property**)malloc(size * sizeof(Property));
						
						for (i=0; i<size; i++)
						{
							//grab the list of parameters
							PyObject *list = PyList_GetItem(arglist,i);
							
							//grab python strings from the list
							PyObject *argname = PyObject_Str(list);
							
							//convert python string to C string
							char *name = PyString_AsString(argname);
							info->mArgs[i] = (Property*)malloc(sizeof(Property));
							info->mArgs[i]->name = static_cast<char*>(malloc(strlen(name)+1));
							strcpy(info->mArgs[i]->name, name);
							
							info->mArgs[i]->value = (Value*)malloc(sizeof(Value));
							
							//try to deduce the argument type
							
							if (i + defaults_offset >= 0) {
								//first check if there is a default value we can use
								defaultvalue = PyTuple_GetItem(defaults, i+defaults_offset);
								const char* type = defaultvalue->ob_type->tp_name;
								
								if (strcmp(type, "int") == 0)
								{
									info->mArgs[i]->value->type = kInteger;
									info->mArgs[i]->value->u.ivalue = PyInt_AsLong(defaultvalue);
								}
								else if (strcmp(type, "float") == 0)
								{
									info->mArgs[i]->value->type = kFloat;
									info->mArgs[i]->value->u.fvalue = (float)PyFloat_AsDouble(defaultvalue);
								}
								else if (strcmp(type, "bool") == 0)
								{
									info->mArgs[i]->value->type = kBoolean;
									info->mArgs[i]->value->u.ivalue = PyInt_AsLong(defaultvalue);
								}
								else // anything from str to tuple, dict, none
								{
									info->mArgs[i]->value->type = kString;
									PyObject *pStr = PyObject_Str(defaultvalue);
									char *str = PyString_AsString(pStr);
									AllocateValueString_(ip, str, info->mArgs[i]->value);
									Py_DECREF(pStr);
								}
							}
							else
							{
								// get the types by chopping after the underscore
								char *temp = PyString_AsString(argname);
								char *delims = "_";
								char *result;
								result = strtok( temp, delims );
								
								while (result != NULL)
								{
									result = strtok( NULL, delims );
									if (result == NULL)
										break;
									else
									{
										if (strcmp(result,"int") == 0)
										{
											info->mArgs[i]->value->type = kInteger;
											info->mArgs[i]->value->u.ivalue = 0;
										}
										else if (strcmp(result,"float") == 0)
										{
											info->mArgs[i]->value->type = kFloat;
											info->mArgs[i]->value->u.fvalue = 0;
										}
										else if (strcmp(result,"bool") == 0)
										{
											info->mArgs[i]->value->type = kBoolean;
											info->mArgs[i]->value->u.ivalue = 0;
										}
										else
										{
											info->mArgs[i]->value->type = kString;
											char *str = "";
											AllocateValueString_(ip, str, info->mArgs[i]->value);
										}
									}
								}
							}
							Py_DECREF(argname);
						}
					}
					Py_DECREF(argspec_tuple);
				}
			}			
		}
	}
	info->mNumArgs = size;

	// Finish the Python Interpreter
	Py_Finalize();
	
	return;
}

// ---------------------------------------------------------------------------------
//		 CallPythonFunc
// ---------------------------------------------------------------------------------

static void
CallPythonFunc(
	IsadoraParameters*	ip,
	ActorInfo* inActorInfo )
{
	PyObject *pName, *pModule, *pDict, *pFunc = NULL, *pValue, *pArgs;
	Value val;
	unsigned int i, size = 0;
	
	// NB: PyObjects returned by PyObject_*, PyNumber_*, PySequence_* or PyMapping_* functions must 
	// be dererefereced using Py_DECREF, PyObjects returned by PyString_*, PyTuple_* etc must not!
	// See https://docs.python.org/2/c-api/intro.html#reference-counts
	
	PluginInfo* info = GetPluginInfo_(inActorInfo);

	// Initialize the python interpreter
	Py_Initialize();
	
	// Make sure we are getting the module from the correct place
	if (info->mPath != NULL && strlen(info->mPath) > 0)
	{
		char *buffer = (char*)malloc( strlen(info->mPath)+25 );
		sprintf(buffer, "sys.path.append(\"%s\")", info->mPath);

		PyRun_SimpleString("import sys");
		PyRun_SimpleString(buffer);
		free(buffer);
	}
	
	// Build the name object
	pName = PyString_FromString(info->mFile);
	if (pName != NULL)
	{
		// Load the module object
		pModule = PyImport_Import(pName);
		if (pModule != NULL)
		{
			pDict = PyModule_GetDict(pModule);
			if (pDict != NULL)
			{
				pFunc = PyDict_GetItemString(pDict, info->mFunc);
			}
		}
	}
	
	if (PyCallable_Check(pFunc))
	{		
		// Set the number of arguments
		pArgs = PyTuple_New(info->mNumArgs);
		
		UInt32 propCount, argCount;
		IzzyError err = GetPropertyCount_(ip, inActorInfo, kInputProperty, &propCount);
	
		argCount = propCount - (kInputArg0-1);
	
		for (i=0; i<info->mNumArgs; i++)
		{
			if (i < argCount) {
				Value *val = GetInputPropertyValue_(ip, inActorInfo, kInputArg0 + i);
				switch(val->type)
				{
				case kInteger:
					PyTuple_SetItem(pArgs, i, PyInt_FromLong(val->u.ivalue));
					break;
				case kFloat:
					PyTuple_SetItem(pArgs, i, PyFloat_FromDouble(val->u.fvalue));
					break;
				case kBoolean:
					PyTuple_SetItem(pArgs, i, PyBool_FromLong(val->u.ivalue));
					break;
				case kString:
					PyTuple_SetItem(pArgs, i, PyString_FromString(val->u.str->strData));
					break;
				}
			}
			else
			{
				PyTuple_SetItem(pArgs, i, Py_None);
			}
		}
		// Make the call to the function
		pValue = PyObject_CallObject(pFunc, pArgs);
		
		// Check for a return value and if its a tuple
		if (pValue != NULL)
		{
			// Show result
			val.type = kString;
			PyObject *pStr = PyObject_Str(pValue);
			AllocateValueString_(ip, PyString_AsString(pStr), &val);
			SetOutputPropertyValue_(ip, inActorInfo, kOutputResult, &val);
			ReleaseValueString_(ip, &val);
			Py_DECREF(pStr);
			
			// Reset error output
			val.type = kString;
			AllocateValueString_(ip, "", &val);
			SetOutputPropertyValue_(ip, inActorInfo, kOutputError, &val);
			ReleaseValueString_(ip, &val);
			
			// Output trigger
			val.type = kBoolean;
			val.u.ivalue = 1;
			SetOutputPropertyValue_(ip, inActorInfo, kOutputTrigger, &val);
			
			Py_DECREF(pValue);
		}
		else
		{
			PyObject *pErrType, *pErrValue, *pTraceback;
			//pErrValue contains error message
			//pTraceback contains stack snapshot and many other information
			//(see python traceback structure)
			
			PyErr_Fetch(&pErrType, &pErrValue, &pTraceback);
			
			val.type = kString;
			if (pErrValue != NULL)
				AllocateValueString_(ip, PyString_AsString(pErrValue), &val);
			else
				AllocateValueString_(ip, "unspecified error", &val);
			SetOutputPropertyValue_(ip, inActorInfo, kOutputError, &val);
			ReleaseValueString_(ip, &val);
		}	
	}
	
	// Finish the Python Interpreter
	Py_Finalize();
}
	
// ---------------------------------------------------------------------------------
//		¥ HandlePropertyChangeValue	[INTERRUPT SAFE]
// ---------------------------------------------------------------------------------
//	This function is called whenever one of the input values of an actor changes.
//	The one-based property index of the input is given by inPropertyIndex1.
//	The new value is given by inNewValue, the previous value by inOldValue.
//
static void
HandlePropertyChangeValue(
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo,
	PropertyIndex		inPropertyIndex1,			// the one-based index of the property than changed values
	ValuePtr			/* inOldValue */,			// the property's old value
	ValuePtr			inNewValue,					// the property's new value
	Boolean				/* inInitializing */)		// true if the value is being set when an actor is first initalized
{
	PluginInfo* info = GetPluginInfo_(inActorInfo);

	bool findFunc = false;
	
	Value outputValue;
	outputValue.type = kFloat;
	
	switch (inPropertyIndex1) {
		
		case kInputTrigger:
			if (info->mFuncFound)
				CallPythonFunc(ip, inActorInfo);
			break;
		
		case kInputPath:
			if (info->mPath != NULL)
			{
				free(info->mPath);
				info->mPath = NULL;
			}
			if (info->mPath == NULL && inNewValue->u.str != NULL)
			{
				info->mPath = static_cast<char*>(malloc(strlen(inNewValue->u.str->strData)+1));
				strcpy(info->mPath, inNewValue->u.str->strData);
			}
			findFunc = true;
			break;
			
		case kInputFile:
			if (info->mFile != NULL)
			{
				free(info->mFile);
				info->mFile = NULL;
			}
			if (info->mFile == NULL && inNewValue->u.str  != NULL)
			{
				info->mFile = static_cast<char*>(malloc(strlen(inNewValue->u.str->strData)+1));
				strcpy(info->mFile, inNewValue->u.str->strData);
			}
			findFunc = true;
			break;
			
		case kInputFunc:
			if (info->mFunc != NULL)
			{
				free(info->mFunc);
				info->mFunc = NULL;
			}
			if (info->mFunc == NULL && inNewValue->u.str  != NULL)
			{
				info->mFunc = static_cast<char*>(malloc(strlen(inNewValue->u.str->strData)+1));
				strcpy(info->mFunc, inNewValue->u.str->strData);
			}
			findFunc = true;
			break;
			
		case kInputGetArgs:
		{
			ClearArgInputProperties(ip, inActorInfo);
			AddArgInputProperties(ip, inActorInfo);
			break;
		}
			
		default:
		{

		}
	}

	if (findFunc)
	{
		FindPythonFunc(ip, info);
				
		// Output a boolean showing if the function was found
		Value fv;
		fv.type = kBoolean;
		fv.u.ivalue = info->mFuncFound;
		SetOutputPropertyValue_(ip, inActorInfo, kOutputFuncFound, &fv);
	}
}

// ---------------------------------------------------------------------------------
//		 AddArgInputProperties
// ---------------------------------------------------------------------------------
// Adds inputs to the actor for the discovered arguments of the Python function

static void AddArgInputProperties(
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo)
{
	PluginInfo* info = GetPluginInfo_(inActorInfo);
	int delta = info->mNumArgs;

	// Dynamically change inputs of actor
	UInt32 propCount;
	IzzyError err = GetPropertyCount_(ip, inActorInfo, kInputProperty, &propCount);
	PluginAssert_(ip, err == kIzzyNoError && propCount >= 1);
				
	UInt32 changeableOutputCount = propCount;
				
	int count = 0;
	if (delta > 0)
	{					
		// get min and max value from the current value input property
		Value valueMin;
		Value valueMax;
		Value valueInit;
		// get current property display format
		PropertyDispFormat availFmts;
		PropertyDispFormat curFmt;
					
		while (delta-- > 0)
		{
			valueInit = *info->mArgs[count]->value;
			valueMin.type = valueInit.type;
			valueMax.type = valueInit.type;
			// Here we have to check to see what type the input is
			if (valueInit.type == kString)
			{							
				GetPropertyMinMax_(ip, inActorInfo, kInputProperty, kInputPath, &valueMin, &valueMax, NULL);
				availFmts = kDisplayFormatText;
				curFmt = kDisplayFormatText;
			}
			else if (valueInit.type == kInteger)
			{
				valueMin.u.ivalue = -2147483647;
				valueMax.u.ivalue = 2147483647;
				availFmts = kDisplayFormatNumber;
				curFmt = kDisplayFormatNumber;
			}
			else if (valueInit.type == kBoolean)
			{
				valueMin.u.ivalue = 0;
				valueMax.u.ivalue = 1;
				availFmts = kDisplayFormatOnOff;
				curFmt = kDisplayFormatOnOff;
			}
			else if (valueInit.type == kFloat)
			{
				valueMin.u.fvalue = -2147483647.f;
				valueMax.u.fvalue = 2147483647.f;
				availFmts = kDisplayFormatNumber;
				curFmt = kDisplayFormatNumber;
			}
						
			int index = changeableOutputCount + 1;
						
			OSType rateType = CreatePropertyID(ip, "in", index);
						
			PropIDT code = CreatePropertyID(ip, "in", index);
						
			err = AddProperty_(ip, inActorInfo,
								kInputProperty,
								rateType,					// the input type
								FOUR_CHAR_CODE(code),		// the input to which we will conform
								info->mArgs[index-kInputArg0]->name,
								availFmts,
								curFmt,
								1,
								&valueMin,
								&valueMax,
								&valueInit);
			PluginAssert_(ip, err == noErr);
						
			CopyPropDefValueSource_(ip, inActorInfo, kInputProperty, 1, kInputProperty, index);
						
			changeableOutputCount++;
			count++;
		}
	}
}

// ---------------------------------------------------------------------------------
//		 AddArgInputProperties
// ---------------------------------------------------------------------------------
// Clears all added input properties

static void ClearArgInputProperties(	
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo)
{
	UInt32 propCount, argCount;
	IzzyError err = GetPropertyCount_(ip, inActorInfo, kInputProperty, &propCount);

	argCount = propCount - (kInputArg0-1);
	// Remove unwanted params
	if (argCount > 0)
	{
		unsigned int i, index = propCount;
		for (i=0; i<argCount; i++)
		{
			err = RemovePropertyProc_(ip, inActorInfo, kInputProperty, index);
			PluginAssert_(ip, err == noErr);

			index--;
		}
	}
}

// ---------------------------------------------------------------------------------
//		¥ GetActorInfo
// ---------------------------------------------------------------------------------
//	This is function is called by to get the actor's class and ID, and to get
//	pointers to the all of the plugin functions declared locally.
//
//	All members of the ActorInfo struct pointed to by outActorParams have been
//	set to 0 on entry. You only need set functions defined by your plugin
//	

EXPORT_ void
GetActorInfo(
	void*				/* inParam */,
	ActorInfo*			outActorParams)
{
	// REQUIRED information
	outActorParams->mActorName							= kActorName;
	outActorParams->mClass								= kActorClass;
	outActorParams->mID									= kActorID;
	outActorParams->mCompatibleWithVersion				= kCurrentIsadoraCallbackVersion;
	
	// REQUIRED functions
	outActorParams->mGetActorParameterStringProc		= GetParameterString;
	outActorParams->mGetActorHelpStringProc				= GetHelpString;
	outActorParams->mCreateActorProc					= CreateActor;
	outActorParams->mDisposeActorProc					= DisposeActor;
	outActorParams->mActivateActorProc					= ActivateActor;
	outActorParams->mHandlePropertyChangeValueProc		= HandlePropertyChangeValue;
	
	// OPTIONAL FUNCTIONS
	outActorParams->mHandlePropertyConnectProc			= NULL;
	outActorParams->mGetActorDefinedAreaProc			= NULL;
	outActorParams->mDrawActorDefinedAreaProc			= NULL;
	outActorParams->mMouseTrackInActorDefinedAreaProc	= NULL;
}
