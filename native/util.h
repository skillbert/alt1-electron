#pragma once

#define WIN32_LEAN_AND_MEAN

#include <napi.h>
#include <string>
#include <Windows.h>
#include <vector>
#include <string>
#include <TlHelp32.h>
#include <codecvt>
#include <locale>
#include <functional>
#include <map>
#include <assert.h>

using std::string;
using std::vector;
using std::wstring;

typedef unsigned char byte;

enum PinEdge { TopLeft = 0, TopRight = 1, BotLeft = 2, BotRight = 3 };
const char* PinEdgeNames[] = { "topleft","topright","botleft","botright" };

//#define v8string(str) (v8::String::NewFromUtf8(isolate, str))
//#define v8string(str) (v8::String::NewFromUtf8(isolate, str).ToLocalChecked())
#define v8string(str) (Nan::New(str).ToLocalChecked())

struct JSRectangle {
	int x;
	int y;
	int width;
	int height;
	JSRectangle() = default;
	JSRectangle(int x, int y, int w, int h) :x(x), y(y), width(w), height(h) {}
	Napi::Object ToJs(Napi::Env env) {
		auto ret = Napi::Object::New(env);
		ret.Set("x", x);
		ret.Set("y", y);
		ret.Set("width", width);
		ret.Set("height", height);
		return ret;
	}
};

struct PluginInstance {
	Napi::FunctionReference* JsOSWindowCtr;
	Napi::FunctionReference* JsPinnedWindowCtr;
};


bool JsArgsInt32(const Napi::CallbackInfo& info, int index, int* out) {
	if (!info[index].IsNumber()) {
		Napi::TypeError::New(info.Env(), "expected argument of type int32 at position " + std::to_string(index)).ThrowAsJavaScriptException();
		return false;
	}
	*out = info[index].As<Napi::Number>().Int32Value();
	return true;
}
bool JsArgsUInt32(const Napi::CallbackInfo& info, int index, unsigned int* out) {
	if (!info[index].IsNumber()) {
		Napi::TypeError::New(info.Env(), "expected argument of type uint32 at position " + std::to_string(index)).ThrowAsJavaScriptException();
		return false;
	}
	*out = info[index].As<Napi::Number>().Uint32Value();
	return true;
}

bool JsArgsString(const Napi::CallbackInfo& info, int index,string& out) {
	if (!info[index].IsString()) {
		Napi::TypeError::New(info.Env(), "expected argument of type string at position " + std::to_string(index)).ThrowAsJavaScriptException();
		return false;
	}
	out = info[index].As<Napi::String>();
	return true;
}

//TODO parameter type of objectwrap
class OSWindow {
public:
	HWND hwnd = 0;
	OSWindow() = default;
	OSWindow(HWND wnd) :hwnd(wnd) {}
	void SetBounds(JSRectangle bounds);
	JSRectangle GetBounds();
	JSRectangle GetClientBounds();
	bool IsValid();
	string GetTitle();
	//TODO why doesn't this have a default implementation? should i use struct?
	bool operator==(OSWindow other);

	static bool OSWindow::FromJsValue(const Napi::Value jsval, OSWindow* out);
};

class JsOSWindow :public Napi::ObjectWrap<JsOSWindow> {
public:
	JsOSWindow(const Napi::CallbackInfo& info) : Napi::ObjectWrap<JsOSWindow>(info) {
		OSWindow wnd;
		if (OSWindow::FromJsValue(info[0], &wnd)) {
			this->inst = wnd;
		}
		else {
			this->inst = 0;
			//Napi::TypeError::New(info.Env(), "Invalid OSWindow arguments").ThrowAsJavaScriptException();
		}
	}
	static Napi::Function GetClass(Napi::Env env) {
		return DefineClass(env, "OSWindow", {
			InstanceMethod("getBounds", &JsOSWindow::JsGetBounds),
			InstanceMethod("getClientBounds", &JsOSWindow::JsGetClientBounds),
			InstanceMethod("setBounds",&JsOSWindow::JsSetBounds),
			InstanceMethod("getTitle",&JsOSWindow::JsGetTitle),
			InstanceMethod("setPinParent",&JsOSWindow::JsSetPinParent),
			InstanceMethod("equals",&JsOSWindow::JsEquals)
			});
	}
	//TODO is there really no better way to construct from c++?
	static Napi::Object Create(Napi::Env env, OSWindow wnd) {
		auto jswnd = env.GetInstanceData<PluginInstance>()->JsOSWindowCtr->New({});
		Napi::ObjectWrap<JsOSWindow>::Unwrap(jswnd)->inst = wnd;
		return jswnd;
	}
	OSWindow GetInstance() { return inst; }
private:
	OSWindow inst;
	Napi::Value JsGetBounds(const Napi::CallbackInfo& info) { return inst.GetBounds().ToJs(info.Env()); }
	Napi::Value JsGetClientBounds(const Napi::CallbackInfo& info) { return inst.GetClientBounds().ToJs(info.Env()); }
	Napi::Value JsGetTitle(const Napi::CallbackInfo& info) { return Napi::String::New(info.Env(), inst.GetTitle()); }
	void JsSetBounds(const Napi::CallbackInfo& info) {
		int x, y, w, h;
		if (!JsArgsInt32(info, 0, &x) || !JsArgsInt32(info, 1, &y) || !JsArgsInt32(info, 2, &w) || !JsArgsInt32(info, 3, &h)) { return; }
		inst.SetBounds(JSRectangle(x, y, w, h));
	}
	Napi::Value JsSetPinParent(const Napi::CallbackInfo& info);
	Napi::Value JsEquals(const Napi::CallbackInfo& info);
};

bool JsArgsOSWindow(const Napi::CallbackInfo& info, int index, OSWindow* out) {
	//TODO check types
	//TODO all kinds of pointer stuff going wrong here?
	*out = Napi::ObjectWrap<JsOSWindow>::Unwrap(info[index].As<Napi::Object>())->GetInstance();
	return true;
}

bool ParseJsRect(const Napi::Value& val, JSRectangle* out) {
	if (!val.IsObject()) { return false; }
	auto rect = val.As<Napi::Object>();
	auto x = rect.Get("x");
	auto y = rect.Get("y");
	auto width = rect.Get("width");
	auto height = rect.Get("height");
	if (!x.IsNumber() || !y.IsNumber() || !width.IsNumber() || !height.IsNumber()) {
		return false;
	}
	*out = JSRectangle(x.As<Napi::Number>().Int32Value(), y.As<Napi::Number>().Int32Value(), width.As<Napi::Number>().Int32Value(), height.As<Napi::Number>().Int32Value());
	return true;
}

Napi::Value JsOSWindow::JsEquals(const Napi::CallbackInfo& info) {
	OSWindow other;
	if (!JsArgsOSWindow(info, 0, &other)) { return Napi::Boolean::New(info.Env(), false); }
	return Napi::Boolean::New(info.Env(), inst == other);
}