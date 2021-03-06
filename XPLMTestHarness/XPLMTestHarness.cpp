// XPLMTestHarness.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "XPLMTestHarness.h"
#include "XPLMScenery.h"
#include "XPLMGraphics.h"
#include <map>
#include <tuple>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#include <Shlwapi.h>

using std::map;
using std::tuple;
using std::make_tuple;
using std::string;
using std::vector;
using std::remove;

static int idSource;

#  define strcp(dest, str) strcpy_s(dest, strlen(str) + 1, str)

// NOTE: The data type naming conventions in this file match those used in the X-Plane API.
// E.g., int is 'i' and int array is 'iv'.  But I defined 'data' as 'bv' (byte vector) instead
// of 'b' like X-Plane does b/c resolving that inconsistency made writing the macros easier.

template <typename T>
struct dataref
{
	dataref(const string n, const T& v)
		: id(++idSource), name(n), value(v)
	{ }

	dataref(const dataref& other)
		: id(other.id), name(other.name), value(other.value)
	{ }

	int id;
	string name;
	T value;

	operator XPLMDataRef ()
	{
		return reinterpret_cast<XPLMDataRef>(static_cast<uintptr_t>(id));
	}

	bool operator == (XPLMDataRef id)
	{
		return static_cast<XPLMDataRef>(*this) == id;
	}
};

struct command
{
	command(const string n, CommandCallback cb)
		: id(++idSource), name(n), callback(cb)
	{ }

	command(const command& other)
		: id(other.id), name(other.name), callback(other.callback)
	{ }

	int id;
	string name;
	CommandCallback callback;

	operator XPLMCommandRef ()
	{
		return reinterpret_cast<XPLMCommandRef>(static_cast<uintptr_t>(id));
	}
};

struct flightloop
{
	flightloop(const XPLMFlightLoop_f fl, float i)
		: flightLoop(fl), interval(i)
	{ }

	flightloop(const flightloop& other)
		: flightLoop(other.flightLoop), interval(other.interval)
	{ }

	XPLMFlightLoop_f flightLoop;
	float interval;
	bool deleted = false;
};

struct drawcallback
{
	drawcallback(const XPLMDrawCallback_f dc, const XPLMDrawingPhase phase, int wb)
		: drawCallback(dc), drawingPhase(phase), wantsBefore(wb) 
	{ }

	XPLMDrawCallback_f drawCallback;
	XPLMDrawingPhase drawingPhase;
	int wantsBefore;
	bool deleted = false;
};

template <typename T>
using datarefmap = map<string, dataref<T>>;

static datarefmap<int> iData;
static datarefmap<vector<int>> ivData;
static datarefmap<float> fData;
static datarefmap<vector<float>> fvData;
static datarefmap<double> dData;
static datarefmap<vector<BYTE>> bvData;

using commandmap = map<string, command>;
static commandmap configuredCommands;

static map<XPLMFlightLoop_f, flightloop> registeredFlightLoops;
static map<tuple<XPLMDrawCallback_f, XPLMDrawingPhase, int>, drawcallback> registeredDrawCallbacks;

template <typename T>
void SetDataRef(const string name, const T& value, datarefmap<T>& container)
{
	auto it = container.find(name);
	if (it == container.end())
		container.emplace(name, dataref<T>(name, value));
	else
		it->second.value = value;
}

template <typename T>
dataref<T>* FindDataRef(XPLMDataRef id, datarefmap<T>& container)
{
	for (auto i = container.begin(), end = container.end(); i != end; ++i)
	{
		if (i->second == id)
			return &i->second;
	}

	return nullptr;
}

command* FindCommand(XPLMCommandRef id, commandmap& container)
{
	for (auto i = container.begin(), end = container.end(); i != end; ++i)
	{
		if (i->second == id)
			return &i->second;
	}

	return nullptr;
}

bool GetEntrypointExecutableAbsolutePath(std::string& entrypointExecutable)
{
	CHAR path[MAX_PATH];
	GetModuleFileNameA(NULL, path, sizeof(path));
	entrypointExecutable = path;
	return true;
}

#define DEFINE_DATA_SET(code, type) \
	XPLM_API void XPHarnessSetDataRef##code (const char* dataRefName, type data) { SetDataRef(dataRefName, data, code##Data); }

#define DEFINE_DATA_SET_VECTOR(code, type) \
	XPLM_API void XPHarnessSetDataRef##code##v (const char* dataRefName, type* data, int size) { SetDataRef(dataRefName, vector<type>(data, data + size), code##vData); }

DEFINE_DATA_SET(i, int)
DEFINE_DATA_SET_VECTOR(i, int)
DEFINE_DATA_SET(f, float)
DEFINE_DATA_SET_VECTOR(f, float)
DEFINE_DATA_SET(d, double)
DEFINE_DATA_SET_VECTOR(b, BYTE)

XPLM_API XPLMPluginID         XPLMGetMyID(void)
{
	return 1;
}

XPLM_API void                 XPLMDebugString(
	const char *         inString)
{
	std::cout << inString;
}

XPLM_API void                 XPLMEnableFeature(
	const char *         inFeature,
	int                  inEnable)
{
	// Not implemented.
}

XPLM_API void                 XPLMGetPluginInfo(
	XPLMPluginID         inPlugin,
	char *               outName,    /* Can be NULL */
	char *               outFilePath,    /* Can be NULL */
	char *               outSignature,    /* Can be NULL */
	char *               outDescription)    /* Can be NULL */
{
	if (outName)
		strcp(outName, "XPLMTestHarness");

	if (outFilePath)
	{
		std::string hostPath;
		GetEntrypointExecutableAbsolutePath(hostPath);
		strcp(outFilePath, hostPath.c_str());
	}

	if (outSignature)
		strcp(outSignature, "1234");

	if (outDescription)
		strcp(outDescription, "Info provided by the test harness for XPNet, which simulates the X-Plane API.");
}

XPLM_API XPLMDataRef          XPLMFindDataRef(
	const char *         inDataRefName)
{
#   define TryReturnDataRef(name, container) { auto it = container.find(name); if (it != container.end()) return it->second; }

	TryReturnDataRef(inDataRefName, iData);
	TryReturnDataRef(inDataRefName, ivData);
	TryReturnDataRef(inDataRefName, fData);
	TryReturnDataRef(inDataRefName, fvData);
	TryReturnDataRef(inDataRefName, dData);
	TryReturnDataRef(inDataRefName, bvData);

	return nullptr;
}

XPLM_API XPLMDataTypeID       XPLMGetDataRefTypes(
	XPLMDataRef          inDataRef)
{
	XPLMDataTypeID types = 0;

	if (FindDataRef(inDataRef, iData))
		types |= xplmType_Int;
	if (FindDataRef(inDataRef, ivData))
		types |= xplmType_IntArray;
	if (FindDataRef(inDataRef, fData))
		types |= xplmType_Float;
	if (FindDataRef(inDataRef, fvData))
		types |= xplmType_FloatArray;
	if (FindDataRef(inDataRef, dData))
		types |= xplmType_Double;
	if (FindDataRef(inDataRef, bvData))
		types |= xplmType_Data;

	return types;
}

XPLM_API int                  XPLMGetDatai(
	XPLMDataRef          inDataRef)
{
	auto d = FindDataRef(inDataRef, iData);
	return d ? d->value : 0;
}

XPLM_API void                 XPLMSetDatai(
	XPLMDataRef          inDataRef,
	int                  inValue)
{
	auto d = FindDataRef(inDataRef, iData);
	if (!d)
		return; // Dataref isn't defined, can't set it.
	d->value = inValue;
}

XPLM_API float                XPLMGetDataf(
	XPLMDataRef          inDataRef)
{
	auto d = FindDataRef(inDataRef, fData);
	return d ? d->value : 0;
}

XPLM_API void                 XPLMSetDataf(
	XPLMDataRef          inDataRef,
	float                inValue)
{
	auto d = FindDataRef(inDataRef, fData);
	if (!d)
		return; // Dataref isn't defined, can't set it.
	d->value = inValue;
}

XPLM_API double               XPLMGetDatad(
	XPLMDataRef          inDataRef)
{
	auto d = FindDataRef(inDataRef, dData);
	return d ? d->value : 0;
}

XPLM_API void                 XPLMSetDatad(
	XPLMDataRef          inDataRef,
	double               inValue)
{
	auto d = FindDataRef(inDataRef, dData);
	if (!d)
		return; // Dataref isn't defined, can't set it.
	d->value = inValue;
}

XPLM_API int                  XPLMGetDatavi(
	XPLMDataRef          inDataRef,
	int *                outValues,    /* Can be NULL */
	int                  inOffset,
	int                  inMax)
{
	auto d = FindDataRef(inDataRef, ivData);
	if (!d)
		return 0;

	if (outValues)
		std::copy_n(d->value.begin(), min(d->value.size(), inMax), outValues + inOffset);

	return static_cast<int>(d->value.size());
}

XPLM_API void                 XPLMSetDatavi(
	XPLMDataRef          inDataRef,
	int *                inValues,
	int                  inoffset,
	int                  inCount)
{
	auto d = FindDataRef(inDataRef, ivData);
	if (!d)
		return; // Dataref isn't defined, can't set it.
	d->value = vector<int>(inValues + inoffset, inValues + inoffset + inCount);
}

XPLM_API int                  XPLMGetDatavf(
	XPLMDataRef          inDataRef,
	float *              outValues,    /* Can be NULL */
	int                  inOffset,
	int                  inMax)
{
	auto d = FindDataRef(inDataRef, fvData);
	if (!d)
		return 0;

	if (outValues)
		std::copy_n(d->value.begin(), min(d->value.size(), inMax), outValues + inOffset);

	return static_cast<int>(d->value.size());
}

XPLM_API void                 XPLMSetDatavf(
	XPLMDataRef          inDataRef,
	float *              inValues,
	int                  inoffset,
	int                  inCount)
{
	auto d = FindDataRef(inDataRef, fvData);
	if (!d)
		return; // Dataref isn't defined, can't set it.
	d->value = vector<float>(inValues + inoffset, inValues + inoffset + inCount);
}

XPLM_API int                  XPLMGetDatab(
	XPLMDataRef          inDataRef,
	void *               outValue,    /* Can be NULL */
	int                  inOffset,
	int                  inMaxBytes)
{
	auto d = FindDataRef(inDataRef, bvData);
	if (!d)
		return 0;

	if (outValue)
		std::copy_n(d->value.begin(), min(d->value.size(), inMaxBytes), reinterpret_cast<BYTE*>(outValue) + inOffset);

	return static_cast<int>(d->value.size());
}

XPLM_API void                 XPLMSetDatab(
	XPLMDataRef          inDataRef,
	void *               inValue,
	int                  inOffset,
	int                  inLength)
{
	auto d = FindDataRef(inDataRef, bvData);
	if (!d)
		return; // Dataref isn't defined, can't set it.
	BYTE* inValueBytes = reinterpret_cast<BYTE*>(inValue);
	d->value = vector<BYTE>(inValueBytes + inOffset, inValueBytes + inOffset + inLength);
}

XPLM_API void XPHarnessSetCommandCallback(const char* commandName, CommandCallback cb)
{
	configuredCommands.emplace(commandName, command(commandName, cb));
}

XPLM_API XPLMCommandRef       XPLMFindCommand(
	const char *         inName)
{
	auto it = configuredCommands.find(inName);
	if (it != configuredCommands.end())
		return it->second;
	return nullptr;
}

XPLM_API void                 XPLMCommandBegin(
	XPLMCommandRef       inCommand)
{
	auto c = FindCommand(inCommand, configuredCommands);
	if (!c)
		return; // Command isn't defined, can't invoke it.

	c->callback(CommandPhase_Begin);
}

XPLM_API void                 XPLMCommandEnd(
	XPLMCommandRef       inCommand)
{
	auto c = FindCommand(inCommand, configuredCommands);
	if (!c)
		return; // Command isn't defined, can't invoke it.

	c->callback(CommandPhase_End);
}

XPLM_API void                 XPLMCommandOnce(
	XPLMCommandRef       inCommand)
{
	auto c = FindCommand(inCommand, configuredCommands);
	if (!c)
		return; // Command isn't defined, can't invoke it.

	c->callback(CommandPhase_Once);
}

XPLM_API float                XPLMGetElapsedTime(void)
{
	return 4242.0f;
}

XPLM_API int                  XPLMGetCycleNumber(void)
{
	return 84848484;
}

XPLM_API void                 XPLMRegisterFlightLoopCallback(
	XPLMFlightLoop_f     inFlightLoop,
	float                inInterval,
	void *               inRefcon)
{
	registeredFlightLoops.emplace(inFlightLoop, flightloop(inFlightLoop, inInterval));
}

XPLM_API void                 XPLMUnregisterFlightLoopCallback(
	XPLMFlightLoop_f     inFlightLoop,
	void *               inRefcon)
{
	auto cb = registeredFlightLoops.find(inFlightLoop);
	cb->second.deleted = true;
}

XPLM_API void                 XPLMSetFlightLoopCallbackInterval(
	XPLMFlightLoop_f     inFlightLoop,
	float                inInterval,
	int                  inRelativeToNow,
	void *               inRefcon)
{
	// ENHANCE: Implement.
}

XPLM_API XPLMFlightLoopID     XPLMCreateFlightLoop(
	XPLMCreateFlightLoop_t * inParams)
{
	// Not implemented b/c we don't use it in XPNet.
	return nullptr;
}

XPLM_API void                 XPLMDestroyFlightLoop(
	XPLMFlightLoopID     inFlightLoopID)
{
	// Not implemented b/c we don't use it in XPNet.
}

XPLM_API void                 XPLMScheduleFlightLoop(
	XPLMFlightLoopID     inFlightLoopID,
	float                inInterval,
	int                  inRelativeToNow)
{
	// Not implemented b/c we don't use it in XPNet.
}

XPLM_API int                  XPLMRegisterDrawCallback(
	XPLMDrawCallback_f   inCallback,
	XPLMDrawingPhase     inPhase,
	int                  inWantsBefore,
	void *               inRefcon)
{
	registeredDrawCallbacks.emplace(make_tuple(inCallback, inPhase, inWantsBefore), drawcallback(inCallback, inPhase, inWantsBefore));
	return 1;
}

XPLM_API int                  XPLMUnregisterDrawCallback(
	XPLMDrawCallback_f   inCallback,
	XPLMDrawingPhase     inPhase,
	int                  inWantsBefore,
	void *               inRefcon)
{
	auto cb = registeredDrawCallbacks.find(make_tuple(inCallback, inPhase, inWantsBefore));
	cb->second.deleted = true;
	return 1;
}

XPLM_API XPLMProbeRef         XPLMCreateProbe(
	XPLMProbeType        inProbeType) 
{
	std::cout << "XPLMTestHarness: Created probe of type " << inProbeType << std::endl;
	return reinterpret_cast<XPLMProbeRef>(static_cast<uintptr_t>(43));
}

XPLM_API void                 XPLMDestroyProbe(
	XPLMProbeRef         inProbe) 
{
	std::cout << "XPLMTestHarness: Destroyed probe with reference " << inProbe << std::endl;
}

XPLM_API XPLMProbeResult      XPLMProbeTerrainXYZ(
	XPLMProbeRef         inProbe,
	float                inX,
	float                inY,
	float                inZ,
	XPLMProbeInfo_t *    outInfo)
{
	std::cout << "XPLMTestHarness: Invoked XPLMProbeTerrainXYZ with X " << inX
		<< ", Y " << inY << ", Z " << inZ << std::endl;
	outInfo->locationX = inX;
	outInfo->locationY = 42;
	outInfo->locationZ = inZ;
	return xplm_ProbeHitTerrain;
}

XPLM_API XPLMObjectRef        XPLMLoadObject(
	const char *         inPath) 
{
	return reinterpret_cast<XPLMObjectRef>(static_cast<uintptr_t>(42));
}

XPLM_API void                 XPLMLoadObjectAsync(
	const char *         inPath,
	XPLMObjectLoaded_f   inCallback,
	void *               inRefcon) 
{
	inCallback(reinterpret_cast<XPLMObjectRef>(static_cast<uintptr_t>(42)), nullptr);
}

XPLM_API void                 XPLMDrawObjects(
	XPLMObjectRef        inObject,
	int                  inCount,
	XPLMDrawInfo_t *     inLocations,
	int                  lighting,
	int                  earth_relative)
{
	std::cout << "XPLMTestHarness: Drawing object " << inObject << std::endl;
}

XPLM_API void                 XPLMUnloadObject(
	XPLMObjectRef        inObject)
{
	std::cout << "XPLMTestHarness: Unloading object " << inObject << std::endl;
}

XPLM_API int                  XPLMLookupObjects(
	const char *         inPath,
	float                inLatitude,
	float                inLongitude,
	XPLMLibraryEnumerator_f enumerator,
	void *               ref)
{
	std::string fullPath ("/realpath/");
	fullPath.append(inPath);
	enumerator(fullPath.c_str(), ref);
	return 1;
}

XPLM_API void                 XPLMWorldToLocal(
	double               inLatitude,
	double               inLongitude,
	double               inAltitude,
	double *             outX,
	double *             outY,
	double *             outZ)
{
	std::cout << "XPLMTestHarness: Invoked XPLMWorldToLocal with lat " << inLatitude
		<< ", lon " << inLongitude << ", alt " << inAltitude << std::endl;
	*outX = inLongitude;
	*outY = inAltitude;
	*outZ = inLatitude;
}

XPLM_API void                 XPLMLocalToWorld(
	double               inX,
	double               inY,
	double               inZ,
	double *             outLatitude,
	double *             outLongitude,
	double *             outAltitude)
{
	std::cout << "XPLMTestHarness: Invoked XPLMLocalToWorld with X " << inX
		<< ", Y " << inY << ", Z " << inZ << std::endl;
	*outLongitude = inX;
	*outAltitude = inY;
	*outLatitude = inZ;
}

XPLM_API void XPHarnessInvokeFlightLoop(float elapsedSinceLastCall, float elapsedTimeSinceLastFlightLoop, int counter)
{
	// Before invoking clean up all the unregistered flight loops.
	auto it = registeredFlightLoops.begin();
	while (it != registeredFlightLoops.end())
	{
		if (it->second.deleted)
			it = registeredFlightLoops.erase(it);
		else
			++it;
	}

	for (auto p = registeredFlightLoops.begin(); p != registeredFlightLoops.end(); ++p)
		p->first(elapsedSinceLastCall, elapsedTimeSinceLastFlightLoop, counter, nullptr);
}

XPLM_API void XPHarnessInvokeDrawCallback()
{
	// Before invoking clean up all the unregistered draw callbacks.
	auto it = registeredFlightLoops.begin();
	while (it != registeredFlightLoops.end())
	{
		if (it->second.deleted)
			it = registeredFlightLoops.erase(it);
		else
			++it;
	}

	for (auto p = registeredDrawCallbacks.begin(); p != registeredDrawCallbacks.end(); ++p)
		p->second.drawCallback(p->second.drawingPhase, p->second.wantsBefore, nullptr);
}
