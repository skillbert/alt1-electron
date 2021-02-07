#pragma once

#define WIN32_LEAN_AND_MEAN

#include <string>
#include <Windows.h>
#include <vector>
#include <nan.h>
#include <string>
#include <TlHelp32.h>
#include <codecvt>
#include <locale>
#include <functional>
#include <map>


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
};
JSRectangle MakeJSRect(int x, int y, int w, int h) {
	return { x,y,w,h };
}


struct OSHandle {
protected:
	uint64_t handle;

public:
	v8::Local<v8::Uint8Array> GetJsPointer(v8::Isolate* isolate) {
		auto buf = v8::ArrayBuffer::New(isolate, sizeof(handle));
		memcpy(buf->GetContents().Data(), &handle, sizeof(handle));
		auto view = v8::Uint8Array::New(buf, 0, sizeof(handle));
		return view;
	}

	uint64_t GetRaw() { return handle; }

	OSHandle(uint64_t v) :handle(v) {}
	OSHandle() = default;
};

struct OSWindow:OSHandle {
	HWND GetHwnd() { return (HWND)handle; }
	OSWindow(HWND hwnd) { handle = (uint64_t)hwnd; }
	OSWindow(OSHandle h) { handle = h.GetRaw(); }
	OSWindow() = default;
};



class OSWindow2:public node::ObjectWrap {
protected:
public:
	OSWindow2() = default;
	virtual void SetBounds(JSRectangle bounds) = 0;
	virtual JSRectangle GetBounds() = 0;
	virtual bool IsValid() = 0;
	virtual string GetTitle() = 0;
	static bool FromJS(const v8::Local<v8::Value> electronhandle, OSWindow2* out);
	static void OSWindow2::Init(v8::Local<v8::Object> target) {
		auto isolate = target->GetIsolate();
		v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate);
		tpl->SetClassName(v8::Symbol::New(isolate, v8string("Foobar")));
		tpl->InstanceTemplate()->SetInternalFieldCount(1);
		// Prototype functions
		tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("getID"), v8::FunctionTemplate::New(nodeGetID)->GetFunction());
		tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("getVelocity"), v8::FunctionTemplate::New(nodeGetVelocity)->GetFunction());
		tpl->PrototypeTemplate()->Set(v8::String::NewSymbol("getState"), v8::FunctionTemplate::New(nodeGetState)->GetFunction());

		v8::Persistent<v8::Function> constructor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
		target->Set(v8::String::NewSymbol("Foobar"), constructor);

	}
};

bool JsOSHandle(const v8::Local<v8::Value> jsval, OSHandle* out) {
	if (!jsval->IsArrayBufferView()) { return { 0 }; }
	auto buf = jsval.As<v8::ArrayBufferView>();
	if (buf->ByteLength() != sizeof(OSWindow)) { return { 0 }; }
	byte* ptr = (byte*)buf->Buffer()->GetContents().Data();
	ptr += buf->ByteOffset();
	*out = OSHandle(*(uint64_t*)ptr);
}

bool JsArgsInt32(const Nan::NAN_METHOD_ARGS_TYPE info, int index, int* out) {
	auto ctx = info.GetIsolate()->GetCurrentContext();
	if (!info[index]->Int32Value(ctx).To(out)) {
		Nan::ThrowTypeError(("Argument " + std::to_string(index) + " is not of type int32").c_str());
		return false;
	}
	return true;
}

bool JsArgsString(const Nan::NAN_METHOD_ARGS_TYPE info, int index, string& out) {
	auto ctx = info.GetIsolate()->GetCurrentContext();
	if (!info[index]->IsString()) {
		Nan::ThrowTypeError(("Argument " + std::to_string(index) + " is not of type string").c_str());
		return false;
	}
	out = string(*Nan::Utf8String(info[0]));
	return true;
}

bool JsArgsOSWindow(const Nan::NAN_METHOD_ARGS_TYPE info, int index, OSWindow* out) {
	OSHandle h;
	if (!JsOSHandle(info[index], &h)) {
		Nan::ThrowTypeError(("Argument " + std::to_string(index) + " is not of type OSWindow").c_str());
		return false;
	}
	*out = OSWindow(h);
	return true;
}