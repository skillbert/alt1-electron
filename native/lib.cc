

#include "jsapi.h"

//TODO remove
Napi::Value test(const Napi::CallbackInfo& info) {
	throw Napi::Error::New(info.Env(), string(info[0].As<Napi::String>()));
}

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
	exports.Set("setWindowShape", Napi::Function::New(env, SetWindowShape));
	exports.Set("unsetWindowShape", Napi::Function::New(env, UnsetWindowShape));

	exports.Set("newWindowListener", Napi::Function::New(env, NewWindowListener));
	exports.Set("removeWindowListener", Napi::Function::New(env, RemoveWindowListener));

	exports.Set("test", Napi::Function::New(env, test));
	return exports;
}

NODE_API_MODULE(hello, Init)
