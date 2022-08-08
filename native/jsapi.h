#include <map>
#include "os.h"
#include "../libs/Alt1Native.h"


const std::map<CaptureMode, std::string> captureModeText = {
	{CaptureMode::Desktop,"desktop"},
	{CaptureMode::Window,"window"},
	{CaptureMode::OpenGL,"opengl"}
};

std::map<OSWindow, Alt1Native::HookedProcess*> hookedWindows;

Napi::Value HookWindow(const Napi::CallbackInfo& info) {
#ifdef OPENGL_SUPPORTED
	auto wnd = OSWindow::FromJsValue(info[0]);

	auto handle = Alt1Native::HookProcess(wnd.handle);
	hookedWindows[wnd] = handle;
	return Napi::BigInt::New(info.Env(), (uintptr_t)handle);
#else
	return Napi::BigInt::New(info.Env(), (uint64_t) 0);
#endif
}

Napi::Value CaptureWindowMulti(const Napi::CallbackInfo& info) {
	auto env = info.Env();
	auto wnd = OSWindow::FromJsValue(info[0]);

	//convert the capture mode string
	auto captmodetext = info[1].As<Napi::String>().Utf8Value();
	bool modematched = false;
	CaptureMode captmode = CaptureMode::Desktop;
	for (auto mode : captureModeText) {
		if (mode.second == captmodetext) {
			modematched = true;
			captmode = mode.first;
			break;
		}
	}
	if (!modematched) {
		throw Napi::RangeError::New(env, "unknown capture mode");
	}

	//convert the capture rect object to c++
	auto obj = info[2].As<Napi::Object>();
	auto props = obj.GetPropertyNames();
	vector<CaptureRect> capts;
	auto ret = Napi::Object::New(env);
	for (uint32_t a = 0; a < props.Length(); a++) {
		auto key = props.Get(a);
		if (!key.IsString() || !obj.HasOwnProperty(key)) { continue; }
		auto val = obj.Get(key);
		if (val.IsNull() || val.IsUndefined()) { continue; }
		auto rect = JSRectangle::FromJsValue(val);
		if (rect.width <= 0 || rect.height <= 0 || rect.width > 1e4 || rect.height > 1e4)
		{
			throw Napi::TypeError::New(env, "invalid capture size");
		}

		size_t size = (size_t)rect.width * rect.height * 4;
		auto buffer = Napi::ArrayBuffer::New(env, size);
		CaptureRect capt(buffer.Data(), buffer.ByteLength(), rect);
		auto view = Napi::Uint8Array::New(env, size, buffer, 0, napi_uint8_clamped_array);
		ret.Set(key, view);
		capts.push_back(capt);
	}
	OSCaptureMulti(wnd, captmode, capts, env);
	return ret;
}

Napi::Value GetRsHandles(const Napi::CallbackInfo& info) {
	auto handles = OSGetRsHandles();
	auto ret = Napi::Array::New(info.Env(), handles.size());
	for (size_t i = 0; i < handles.size(); i++) { ret.Set(i, handles[i].ToJS(info.Env())); }
	return ret;
}

Napi::Value JSGetActiveWindow(const Napi::CallbackInfo& info) { return OSGetActiveWindow().ToJS(info.Env()); }
Napi::Value GetWindowBounds(const Napi::CallbackInfo& info) { return OSWindow::FromJsValue(info[0]).GetBounds().ToJs(info.Env()); }
Napi::Value GetClientBounds(const Napi::CallbackInfo& info) { return OSWindow::FromJsValue(info[0]).GetClientBounds().ToJs(info.Env()); }
Napi::Value GetWindowTitle(const Napi::CallbackInfo& info) { return Napi::String::New(info.Env(), OSWindow::FromJsValue(info[0]).GetTitle()); }
void SetWindowParent(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto parent = OSWindow::FromJsValue(info[1]);
	OSSetWindowParent(wnd, parent);
}

void SetWindowShape(const Napi::CallbackInfo& info) {
	auto arr = info[1].As<Napi::Array>();
	uint8_t op = static_cast<uint8_t>(info[2].As<Napi::Number>().Int32Value());
	std::vector<JSRectangle> rects;
	rects.reserve(arr.Length());
	for(uint32_t i = 0; i < arr.Length(); i++) {
		rects.push_back(JSRectangle::FromJsValue(arr[i]));
	}
	OSSetWindowShape(OSWindow::FromJsValue(info[0]), rects, op);
}

void UnsetWindowShape(const Napi::CallbackInfo& info) { OSUnsetWindowShape(OSWindow::FromJsValue(info[0])); }

void NewWindowListener(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto typestring = info[1].As<Napi::String>().Utf8Value();
	Napi::Function cb = info[2].As<Napi::Function>();
	auto typefind= windowEventTypes.find(typestring);
	if (typefind == windowEventTypes.end()) {
		throw Napi::RangeError::New(info.Env(), "unknown event type");
	}
	OSNewWindowListener(wnd, typefind->second, cb);
}

void RemoveWindowListener(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto typestring = info[1].As<Napi::String>().Utf8Value();
	Napi::Function cb = info[2].As<Napi::Function>();
	auto typefind = windowEventTypes.find(typestring);
	if (typefind == windowEventTypes.end()) {
		throw Napi::RangeError::New(info.Env(), "unknown event type");
	}
	OSRemoveWindowListener(wnd, typefind->second, cb);
}
