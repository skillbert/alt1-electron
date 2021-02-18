#pragma once


#include <napi.h>
#include <string>
#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <functional>
#include <map>
#include <assert.h>
#include <unordered_map>
#include <list>

using std::string;
using std::vector;
using std::wstring;

typedef unsigned char byte;

//state storage per context
struct PluginInstance {};

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
	static JSRectangle FromJsValue(const Napi::Value& val) {
		auto rect = val.As<Napi::Object>();
		int x = rect.Get("x").As<Napi::Number>().Int32Value();
		int y = rect.Get("y").As<Napi::Number>().Int32Value();
		int w = rect.Get("width").As<Napi::Number>().Int32Value();
		int h = rect.Get("height").As<Napi::Number>().Int32Value();
		return JSRectangle(x, y, w, h);
	}
};

//TODO this should never be needed
void flipBGRAtoRGBA(void* data, size_t len) {
	byte* index = (byte*)data;
	byte* end = index + len;
	for (; index < end; index += 4) {
		unsigned char tmp = index[0];
		//TODO profile this, does the compiler do fancy simd swizzles if we self assign 1 and 3 as well?
		index[0] = index[2];
		index[2] = tmp;
	}
}

void flipBGRAtoRGBA(void* outdata, void* indata, size_t len) {
	byte* inbytes = (byte*)indata;
	byte* outbytes = (byte*)outdata;
	for (size_t i = 0; i < len; i += 4) {
		outbytes[i + 0] = inbytes[i + 2];
		outbytes[i + 1] = inbytes[i + 1];
		outbytes[i + 2] = inbytes[i + 0];
		outbytes[i + 3] = inbytes[i + 3];
	}
}