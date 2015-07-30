// ===========================================================================
//	Isadora Python Plugin							©2015 Aldo Hooeben,
//													©2003 Mark F. Coniglio. 
//													All rights reserved.
// ===========================================================================
//
//	Based on ExecutePythonFunction.cpp ©2003 Mark F. Coniglio.
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
// ### Declare global variables, common to all instantiations of this plugin here


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
// ### This structure neeeds to contain all variables used by your plugin. Memory for
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
	bool				mLoadArgs;
	
	int					mNumArgs;
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

// ### GROUP ID
// Define the group under which this plugin will be displayed in the Isadora interface.
// These are defined under "Actor Types" in IsadoraTypes.h

static const OSType	kActorClass 	= kGroupControl;

// ### PLUGIN IN
// Define the plugin's unique four character identifier. Contact TroikaTronix to
// obtain a unique four character code if you want to ensure that someone else
// has not developed a plugin with the same code. Note that TroikaTronix reserves
// all plugin codes that begin with an underline, an at-sign, and a pound sign
// (e.g., '_', '@', and '#'.)

static const OSType	kActorID		= FOUR_CHAR_CODE('0OtC');

// ### ACTOR NAME
// The name of the actor. This is the name that will be shown in the User Interface.

static const char* kActorName		= "PythonPlugin";

// ### PROPERTY DEFINITION STRING
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
	"INPROP		params			parm	bool		onoff				0		1		0\r"

// OUTPUT PROPERTY DEFINITIONS
//	TYPE 		PROPERTY NAME	ID		DATATYPE	DISPLAY FMT			MIN		MAX		INIT VALUE
	"OUTPROP 	function_found	fnd		bool		onoff				0		1		0\r"
	"OUTPROP 	function_ran	ran		bool		trig				0		1		0\r"
	"OUTPROP	error			err		string		text				*		*		\r"
	"OUTPROP	output			out		string		text				*		*		\r";

// ### Property Index Constants
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
	kInputParams,
	kInputArg0,
	
	kOutputFuncFound = 1,
	kOutputTrigger,
	kOutputError,
	kOutputResult
};


// ---------------------
//	Help String
// ---------------------
// ### Help Strings
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
	
	"If set to 'on', the python properties are added for function arguments.",
	
	"Argument for the python function. "
	"Note that this input is mutable: it changes its type to match the output property to which it is linked.",

	"Set to 'on' if the specified python function was found.",
	
	"Triggered when the function has succesfully executed.",

	"Outputs any error string returned by the python function. ",

	"Outputs data returned by the python function. "
	"Note that this input is mutable: it changes its type to match the input property to which it is linked.",
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
	
	// ### destruction of private member variables
	if (info->mPath != NULL)
		free(info->mPath);
	if (info->mFile != NULL)
		free(info->mFile);
	if (info->mFunc != NULL)
		free(info->mFunc);
	
	if (info->mArgs != NULL)
	{
		int i, size;
		size = sizeof(info->mArgs);
		for ( i=0; i<size; i++)
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
	if (inActivate) {
	
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
	
	} else {
	
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
	if (inPropertyType == kInputProperty) {
		if (inPropertyIndex1 >= kInputArg0) {
			inPropertyIndex1 = kInputArg0;
		}
	} 
	
	// The PropertyTypeAndIndexToHelpIndex_ converts the inPropertyType and
	// inPropertyIndex1 parameters to determine the zero-based index into
	// your list of help strings.
	UInt32 index1 = PropertyTypeAndIndexToHelpIndex_(ip, inActorInfo, inPropertyType, inPropertyIndex1);
	
	if (inPropertyType == kOutputProperty) {
		index1 = kInputArg0+inPropertyIndex1;
	}

	// get the help string
	helpstr = sHelpStrings[index1];
	
	// copy it to the output string
	strncpy(outParamaterString, helpstr, inMaxCharacters);
}

// ---------------------------------------------------------------------------------
//		? CreatePropertyID	[INTERRUPT SAFE]
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
	if (inIndex >= 0 && inIndex < 100) {
		
		indexMS = inIndex / 10;
		indexLS = inIndex % 10;
		
		result |= ( (((UInt32) (indexMS + '0')) << 8) | (((UInt32) (indexLS + '0')) << 0) );
		
		// if between 100 and 776
	} else if (inIndex >= 100 && inIndex < 100 + kTwoCharMax) {
		
		indexOffset = inIndex - 100;
		PluginAssert_(ip, indexOffset >= 0 && indexOffset < kTwoCharMax);
		indexMS = indexOffset / kOneCharMax;
		indexLS = indexOffset % kOneCharMax;
		
		result |= ( (((UInt32) (indexMS + 'A')) << 8) | (((UInt32) (indexLS + 'A')) << 0) );
		
		// if between 776 and 1452
	} else if (inIndex >= 100 + kTwoCharMax && inIndex < 100 + kTwoCharMax * 2) {
		
		indexOffset = inIndex - (100 + kTwoCharMax);
		PluginAssert_(ip, indexOffset >= 0 && indexOffset < kTwoCharMax);
		indexMS = indexOffset / kOneCharMax;
		indexLS = indexOffset % kOneCharMax;
		
		result |= ( (((UInt32) (indexMS + 'a')) << 8) | (((UInt32) (indexLS + 'a')) << 0) );
		
	} else {
		PluginAssert_(ip, false);
	}
	
	return result;
}

// ---------------------------------------------------------------------------------
//		 FindPythonFunc
// ---------------------------------------------------------------------------------
static int
FindPythonFunc(
	IsadoraParameters*	ip,
	PluginInfo* info )
{	
	PyObject *pName, *pModule, *pDict, *pFunc = NULL, *pInspect, *argspec_tuple, *arglist, *defaults, *defaultvalue;
	int size = 0, i;
	
	if (info->mArgs != NULL)
	{
		// free memory for previously created args
		int i, size;
		size = sizeof(info->mArgs);
		for ( i=0; i<size; i++)
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
	}

	// Initialize the python interpreter
	Py_Initialize();
	
	// Make sure we are getting the module from the correct place
	if(info->mPath != NULL && strlen(info->mPath) > 0)
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
		Py_DECREF(pName);
		if (pModule != NULL)
		{
			// pDict is a borrowed reference
			pDict = PyModule_GetDict(pModule);
			Py_DECREF(pModule);
			if (pDict != NULL)
			{
				// pFunc is a borrowed reference
				pFunc = PyDict_GetItemString(pDict, info->mFunc);
			}
		}
	}
	
	info->mFuncFound = (pFunc != NULL && PyCallable_Check(pFunc));
	
	pName = PyString_FromString("inspect");	
	if (pName != NULL)
	{
		pInspect = PyImport_Import(pName);
		Py_DECREF(pName);
		if (pInspect != NULL)
		{
			pName = PyString_FromString("getargspec");
			if (pName != NULL)
			{
				argspec_tuple = PyObject_CallMethodObjArgs(pInspect, pName, pFunc, NULL);
				Py_DECREF(pName);
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
						
						for ( i=0; i<size; i++)
						{
							//grab the list of parameters
							PyObject *list = PyList_GetItem(arglist,i);
							
							//grab python strings from the list
							PyObject *argname = PyObject_Str(list);
							Py_DECREF(list);
							
							//convert python string to C string
							char *name = PyString_AsString(argname);
							info->mArgs[i] = (Property*)malloc(sizeof(Property));
							info->mArgs[i]->name = static_cast<char*>(malloc(strlen(name)+1));
							strcpy(info->mArgs[i]->name, name);
							
							info->mArgs[i]->value = (Value*)malloc(sizeof(Value));
							
							//try to deduce the argument type
							
							if (i+defaults_offset >= 0) {
								//first check if there is a default value we can use
								defaultvalue = PyTuple_GetItem(defaults, i+defaults_offset);
								const char* type = defaultvalue->ob_type->tp_name; 
								if( strcmp(type, "int") == 0 )
								{
									info->mArgs[i]->value->type = kInteger;
									info->mArgs[i]->value->u.ivalue = PyInt_AsLong(defaultvalue);
								} 
								else if( strcmp(type, "float") == 0 )
								{
									info->mArgs[i]->value->type = kFloat;
									info->mArgs[i]->value->u.fvalue = PyFloat_AsDouble(defaultvalue);
								} 
								else if( strcmp(type, "bool") == 0 )
								{
									info->mArgs[i]->value->type = kBoolean;
									info->mArgs[i]->value->u.ivalue = PyInt_AsLong(defaultvalue);
								} 
								else // anything from str to tuple, dict, none
								{
									info->mArgs[i]->value->type = kString;
									char *str = PyString_AsString(PyObject_Str(defaultvalue));
									AllocateValueString_(ip, str, info->mArgs[i]->value);
								}
							} else
							{
								// get the types by chopping after the underscore
								char *temp = PyString_AsString(argname);
								char *delims = "_";
								char *result;
								result = strtok( temp, delims );
							
								// ### CHECK THIS STRUCTURE
								while( result != NULL )
								{
									result = strtok( NULL, delims );
									if (result == NULL)
									{
										break;
									}
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
						Py_DECREF(arglist);
						Py_DECREF(defaults);
					}
					Py_DECREF(argspec_tuple);
				}
			}
			Py_DECREF(pInspect);
		}
	}
	
	// Clean up
	if(pFunc != NULL) Py_DECREF(pFunc);
	
	// Finish the Python Interpreter
	Py_Finalize();
	
	return size;
}

// ---------------------------------------------------------------------------------
//		 CallPythonFunc
// ---------------------------------------------------------------------------------
// Returns the length of the tuple
static void
CallPythonFunc(
	IsadoraParameters*	ip,
	ActorInfo* inActorInfo )
{
	PyObject *pName, *pModule, *pDict, *pFunc = NULL, *pValue, *pArgs;
	Value val;
	int i, size = 0;
	float ret;

	PluginInfo* info = GetPluginInfo_(inActorInfo);

	// Initialize the python interpreter
	Py_Initialize();
	
	// Make sure we are getting the module from the correct place
	if(info->mPath != NULL && strlen(info->mPath) > 0)
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
		Py_DECREF(pName);
		if (pModule != NULL)
		{
			// pDict is a borrowed reference
			pDict = PyModule_GetDict(pModule);
			Py_DECREF(pModule);
			if (pDict != NULL)
			{
				// pFunc is a borrowed reference
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
			if (i<argCount) {
				switch(info->mArgs[i]->value->type)
				{
				case kInteger:
					PyTuple_SetItem(pArgs, i, PyInt_FromLong(info->mArgs[i]->value->u.ivalue));
					break;
				case kFloat:
					PyTuple_SetItem(pArgs, i, PyFloat_FromDouble(info->mArgs[i]->value->u.fvalue));
					break;
				case kBoolean:
					PyTuple_SetItem(pArgs, i, PyBool_FromLong(info->mArgs[i]->value->u.ivalue));
					break;
				case kString:
					PyTuple_SetItem(pArgs, i, PyString_FromString(info->mArgs[i]->value->u.str->strData));
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
		Py_DECREF(pArgs);
		
		// Check for a return value and if its a tuple
		if (pValue != NULL)
		{
			// Show result
			val.type = kString;
			AllocateValueString_(ip, PyString_AsString(PyObject_Str(pValue)), &val);
			SetOutputPropertyValue_(ip, inActorInfo, kOutputResult, &val);
			ReleaseValueString_(ip, &val);
			
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
			
			Py_DECREF(pErrType);
			Py_DECREF(pErrValue);
			Py_DECREF(pTraceback);
		}
	
	}
	
	// Clean up
	if(pFunc != NULL) Py_DECREF(pFunc);
	
	// Finish the Python Interpreter
	Py_Finalize();
}
	
// ---------------------------------------------------------------------------------
//		¥ HandlePropertyChangeValue	[INTERRUPT SAFE]
// ---------------------------------------------------------------------------------
//	### This function is called whenever one of the input values of an actor changes.
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

	// ### When you add/change/remove properties, you will need to add cases
	// to this switch statement, to process the messages for your
	// input properties
	
	bool findFunc = false;
	
	Value outputValue;
	outputValue.type = kFloat;
	
	switch (inPropertyIndex1) {
		
		case kInputTrigger:
			if (info->mFuncFound)
			{
				CallPythonFunc(ip, inActorInfo);
			}
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
			
		case kInputParams:
		{
			info->mLoadArgs = inNewValue->u.ivalue;
			
			if ( info->mFuncFound )
			{
				if ( info->mLoadArgs == 1 )
				{
					AddArgInputProperties(ip, inActorInfo);
				}
				else
				{
					ClearArgInputProperties(ip, inActorInfo);
				}
			}
			break;
		}
			
		default:
		{

		}
	}

	if (findFunc) {
		if (info->mFile != NULL && strlen(info->mFile) > 0 && info->mFunc != NULL && strlen(info->mFunc) > 0) {
			// Find the function and number of parameters at the specified path
			info->mNumArgs = FindPythonFunc(ip, info);
		} else {
			info->mFuncFound = false;
			info->mNumArgs = 0;
		}
				
		// Output a boolean showing if the function was found
		Value fv;
		fv.type = kBoolean;
		fv.u.ivalue = info->mFuncFound;
		SetOutputPropertyValue_(ip, inActorInfo, kOutputFuncFound, &fv);
	}
}


static void AddArgInputProperties(
	IsadoraParameters*	ip,
	ActorInfo*			inActorInfo) 
{
	PluginInfo* info = GetPluginInfo_(inActorInfo);
	int delta = info->mNumArgs;

	/*
	// Output test string
	Value v;
	v.type = kString;
	AllocateValueString_(ip, "test", &v);
	SetOutputPropertyValue_(ip, inActorInfo, kOutputResult, &v);
	ReleaseValueString_(ip, &v);
	*/

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
				valueMin.u.fvalue = -2147483647;
				valueMax.u.fvalue = 2147483647;
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
		unsigned int index = propCount;
		int i;
		for (i=0; i<argCount; i++)
		{
			err = RemovePropertyProc_(ip, inActorInfo, kInputProperty, index);
			PluginAssert_(ip, err == noErr);

			index--;
		}
	}
}

// ---------------------------------------------------------------------------------
//		? PropertyValueToString
// ---------------------------------------------------------------------------------
//	Converts PROPDEF parameters into more meaningful strings

static Boolean PropValueCheck(IsadoraParameters* &ip, ValuePtr &inValue, char* &outString, UInt8 maxValues, char** sList){
	if (inValue->u.ivalue >= 0 && inValue->u.ivalue <= maxValues-1) {
		strcpy(outString, sList[inValue->u.ivalue]);
	} else {
		PluginAssert_(ip, false);
	}
	return true;
}

// ---------------------------------------------------------------------------------
//		? PropertyStringToValue
// ---------------------------------------------------------------------------------
static Boolean PropStringCheck(IsadoraParameters* &ip, const char* &inString, ValuePtr &outValue, UInt8 maxValues, char** sList){
	if (strlen(inString) == 1 && inString[0] >= '0' && inString[0] <= maxValues-1+'0') {
		outValue->type = kInteger;
		outValue->u.ivalue = (SInt32) inString[0] - '0';
		return true;
	} else {
		SInt32 matchIndex = LookupPartialStringInList_(ip, maxValues, sList, inString);
		if (matchIndex >= 0) {
			outValue->type = kInteger;
			outValue->u.ivalue = matchIndex;
			return true;
		}
	}
	return false;
}


// ---------------------------------------------------------------------------------
//		¥ GetActorDefinedArea
// ---------------------------------------------------------------------------------
//	If the mGetActorDefinedAreaProc in the ActorInfo struct points to this function,
//	it indicates to Isadora that the object would like to draw either an icon or else
//	an graphic representation of its function.
//
//	### This function uses the 'PICT' 0 resource stored with the plugin to draw an icon.
//  You should replace this picture (located in the Plugin Resources.rsrc file) with
//  the icon for your actor.
// 
static ActorPictInfo	gPictInfo = { false, nil, nil, 0, 0 };

static Boolean
GetActorDefinedArea(
	IsadoraParameters*			ip,			
	ActorInfo*					inActorInfo,
	SInt16*						outTopAreaWidth,			// returns the width to reserve for the top Actor Defined Area
	SInt16*						outTopAreaMinHeight,		// returns the minimum height of the top area
	SInt16*						outBotAreaHeight,			// returns the width to reserve for the bottom Actor Defined Area
	SInt16*						outBotAreaMinWidth)			// returns the minimum width of the bottom area
{
	if (!gPictInfo.mInitialized) {
		PrepareActorDefinedAreaPict_(ip, inActorInfo, 0, &gPictInfo);
	}
	
	// place picture in top area
	*outTopAreaWidth = gPictInfo.mWidth;
	*outTopAreaMinHeight = gPictInfo.mHeight;
	
	// don't draw anything in bottom area
	*outBotAreaHeight = 0;
	*outBotAreaMinWidth = 0;
	
	return true;
}

// ---------------------------------------------------------------------------------
//		¥ DrawActorDefinedArea
// ---------------------------------------------------------------------------------
//	If GetActorDefinedArea is defined, then this function will be called whenever
//	your ActorDefinedArea needs to be drawn.
//
//	Beacuse we are using the PICT 0 resource stored with this plugin, we can use
//	the DrawActorDefinedAreaPict_ supplied by the Isadora callbacks.
//
//  DrawActorDefinedAreaPict_ is Alpha Channel aware, so you can have nice
//	shading if you like.

static void
DrawActorDefinedArea(
	IsadoraParameters*			ip,
	ActorInfo*					inActorInfo,
	void*						/* inDrawingContext */,		// unused at present
	ActorDefinedAreaPart		inActorDefinedAreaPart,		// the part of the actor that needs to be drawn
	ActorAreaDrawFlagsT			/* inAreaDrawFlags */,		// actor draw flags
	Rect*						inADAArea,					// rect enclosing the entire Actor Defined Area
	Rect*						/* inUpdateArea */,			// subset of inADAArea that needs updating
	Boolean						inSelected)					// TRUE if actor is currently selected
{
	if (inActorDefinedAreaPart == kActorDefinedAreaTop && gPictInfo.mInitialized) {
		DrawActorDefinedAreaPict_(ip, inActorInfo, inSelected, inADAArea, &gPictInfo);
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
	outActorParams->mGetActorDefinedAreaProc			= GetActorDefinedArea;
	outActorParams->mDrawActorDefinedAreaProc			= DrawActorDefinedArea;
	outActorParams->mMouseTrackInActorDefinedAreaProc	= NULL;
}
