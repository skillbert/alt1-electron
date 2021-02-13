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

enum PinEdge { None = 0, Left = 1 << 0, Top = 1 << 1, Right = 1 << 2, Bot = 1 << 3 };
enum PinMode { Auto = 0, Cover = 1 };

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
		int x = info[0].As<Napi::Number>().Int32Value();
		int y = info[1].As<Napi::Number>().Int32Value();
		int w = info[2].As<Napi::Number>().Int32Value();
		int h = info[3].As<Napi::Number>().Int32Value();
		inst.SetBounds(JSRectangle(x, y, w, h));
	}
	Napi::Value JsSetPinParent(const Napi::CallbackInfo& info);
	Napi::Value JsEquals(const Napi::CallbackInfo& info);
};

OSWindow JsArgsOSWindow(const Napi::Value& val) {
	return Napi::ObjectWrap<JsOSWindow>::Unwrap(val.As<Napi::Object>())->GetInstance();
}

JSRectangle ParseJsRect(const Napi::Value& val) {
	auto rect = val.As<Napi::Object>();
	int x = rect.Get("x").As<Napi::Number>().Int32Value();
	int y = rect.Get("y").As<Napi::Number>().Int32Value();
	int w = rect.Get("width").As<Napi::Number>().Int32Value();
	int h = rect.Get("height").As<Napi::Number>().Int32Value();
	return JSRectangle(x, y, w, h);
}

Napi::Value JsOSWindow::JsEquals(const Napi::CallbackInfo& info) {
	OSWindow other = JsArgsOSWindow(info[0]);
	return Napi::Boolean::New(info.Env(), inst == other);
}