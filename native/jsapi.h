
//TODO find the actual defines that node-gyp uses
#ifdef WIN32
#include "oswin.h"
#endif


enum class CaptureMode { Desktop = 0, Window = 1, OpenGL = 2 };
const char* captureModeText[] = { "desktop","window","opengl" };


CaptureMode capturemode = CaptureMode::Window;

void CaptureWindowMultiAuto(OSWindow wnd, CaptureMode mode, vector<CaptureRect> rects, Napi::Env env) {
	for (const auto& capt : rects) {
		if (capt.rect.width <= 0 || capt.rect.height <= 0 || capt.rect.width > 1e4 || capt.rect.height > 1e4)
		{
			Napi::TypeError::New(env, "invalid capture size").ThrowAsJavaScriptException();
		}
	}
	switch (mode) {
	case CaptureMode::Desktop: {
		//TODO double check and document desktop 0 special case
		auto offset = wnd.GetClientBounds();
		auto mapped = vector<CaptureRect>(rects);
		for (auto& capt : mapped) {
			capt.rect.x += offset.x;
			capt.rect.y += offset.y;
		}
		OSCaptureDesktopMulti(rects);
		break;
	}
	case CaptureMode::Window:
		OSCaptureWindowMulti(wnd, rects);
		break;
	default:
		Napi::RangeError::New(env, "No capture mode selected").ThrowAsJavaScriptException();
	}
}

Napi::Value CaptureWindow(const Napi::CallbackInfo& info) {
	auto env = info.Env();
	OSWindow wnd = OSWindow::FromJsValue(info[0]);
	int x = info[1].As<Napi::Number>().Int32Value();
	int y = info[2].As<Napi::Number>().Int32Value();
	int w = info[3].As<Napi::Number>().Int32Value();
	int h = info[4].As<Napi::Number>().Int32Value();

	int totalbytes = w * h * 4;
	auto buf = Napi::ArrayBuffer::New(env, totalbytes);
	vector<CaptureRect> capt = { CaptureRect(buf.Data(),buf.ByteLength(),JSRectangle(x,y,w,h)) };
	CaptureWindowMultiAuto(wnd, capturemode, capt, env);
	auto arr = Napi::Uint8Array::New(env, totalbytes, buf, 0, napi_uint8_clamped_array);
	return arr;
}

Napi::Value CaptureWindowMulti(const Napi::CallbackInfo& info) {
	auto env = info.Env();
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto obj = info[1].As<Napi::Object>();
	auto props = obj.GetPropertyNames();
	vector<CaptureRect> capts;
	auto ret = Napi::Object::New(env);
	for (uint32_t a = 0; a < props.Length(); a++) {
		auto key = props.Get(a);
		if (!key.IsString() || !obj.HasOwnProperty(key)) { continue; }
		auto val = obj.Get(key);
		if (val.IsNull() || val.IsUndefined()) { continue; }
		auto rect = JSRectangle::FromJsValue(val);
		size_t size = (size_t)rect.width * rect.height * 4;
		auto buffer = Napi::ArrayBuffer::New(env, size);
		CaptureRect capt(buffer.Data(), buffer.ByteLength(), rect);
		auto view = Napi::Uint8Array::New(env, size, buffer, 0, napi_uint8_clamped_array);
		ret.Set(key, view);
		capts.push_back(capt);
	}
	CaptureWindowMultiAuto(wnd, capturemode, capts, env);
	return ret;
}

Napi::Value GetProcessMainWindow(const Napi::CallbackInfo& info) {
	uint32_t pid = info[0].As<Napi::Number>().Uint32Value();
	return OSFindMainWindow(pid).ToJS(info.Env());
}

Napi::Value GetProcessesByName(const Napi::CallbackInfo& info) {
	string name = info[0].As<Napi::String>().Utf8Value();
	auto pids = OSGetProcessesByName(name, NULL);
	auto ret = Napi::Array::New(info.Env(), pids.size());
	for (int i = 0; i < pids.size(); i++) { ret.Set(i, pids[i]); }
	return ret;
}

Napi::Value GetWindowBounds(const Napi::CallbackInfo& info) { return OSWindow::FromJsValue(info[0]).GetBounds().ToJs(info.Env()); }
Napi::Value GetClientBounds(const Napi::CallbackInfo& info) { return OSWindow::FromJsValue(info[0]).GetClientBounds().ToJs(info.Env()); }
Napi::Value GetWindowTitle(const Napi::CallbackInfo& info) { return Napi::String::New(info.Env(), OSWindow::FromJsValue(info[0]).GetTitle()); }
void SetWindowBounds(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	int x = info[1].As<Napi::Number>().Int32Value();
	int y = info[2].As<Napi::Number>().Int32Value();
	int w = info[3].As<Napi::Number>().Int32Value();
	int h = info[4].As<Napi::Number>().Int32Value();
    wnd.SetBounds(JSRectangle(x, y, w, h));
}
void SetWindowParent(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto parent = OSWindow::FromJsValue(info[1]);
	OSSetWindowParent(wnd, parent);
}

void NewWindowListener(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto typestring = info[1].As<Napi::String>().Utf8Value();
	Napi::Function cb = info[2].As<Napi::Function>();
	auto typefind= windowEventTypes.find(typestring);
	if (typefind == windowEventTypes.end()) {
		Napi::RangeError::New(info.Env(), "unknown event type").ThrowAsJavaScriptException();
	}
	OSNewWindowListener(wnd, typefind->second, cb);
}

void RemoveWindowListener(const Napi::CallbackInfo& info) {
	auto wnd = OSWindow::FromJsValue(info[0]);
	auto typestring = info[1].As<Napi::String>().Utf8Value();
	Napi::Function cb = info[2].As<Napi::Function>();
	auto typefind = windowEventTypes.find(typestring);
	if (typefind == windowEventTypes.end()) {
		Napi::RangeError::New(info.Env(), "unknown event type").ThrowAsJavaScriptException();
	}
	OSRemoveWindowListener(wnd, typefind->second, cb);
}
