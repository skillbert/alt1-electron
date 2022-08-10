

#include "jsapi.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	auto inst = new PluginInstance();
	//TODO need delete destructor to get rid of the mem again?
	env.SetInstanceData<>(inst);

	exports.Set("captureWindowMulti", Napi::Function::New(env, CaptureWindowMulti));
	exports.Set("getRsHandles", Napi::Function::New(env, GetRsHandles));
	exports.Set("getWindowBounds", Napi::Function::New(env, GetWindowBounds));
	exports.Set("getClientBounds", Napi::Function::New(env, GetClientBounds));
	exports.Set("getWindowTitle", Napi::Function::New(env, GetWindowTitle));
	exports.Set("setWindowParent", Napi::Function::New(env, SetWindowParent));
	exports.Set("getActiveWindow", Napi::Function::New(env, JSGetActiveWindow));
	exports.Set("getMouseState", Napi::Function::New(env, GetMouseState));
	exports.Set("setWindowShape", Napi::Function::New(env, SetWindowShape));

	exports.Set("newWindowListener", Napi::Function::New(env, NewWindowListener));
	exports.Set("removeWindowListener", Napi::Function::New(env, RemoveWindowListener));
	return exports;
}

NODE_API_MODULE(hello, Init)
