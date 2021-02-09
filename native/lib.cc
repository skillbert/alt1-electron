

//todo if windows
#include "oswin.h"
#include "pinnedwindow.h"


Napi::Value captureDesktop(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();
	auto q = info[0];
	int x, y, w, h;
	if (!JsArgsInt32(info, 0, &x) || !JsArgsInt32(info, 1, &y) || !JsArgsInt32(info, 2, &w) || !JsArgsInt32(info, 3, &h)) {
		return env.Undefined();
	}

	if (w <= 0 || h <= 0 || w > 1e4 || h > 1e4)
	{
		Napi::TypeError::New(info.Env(), "invalid capture size").ThrowAsJavaScriptException();
		return env.Undefined();
	}

	int totalbytes = w * h * 4;
	auto buf = Napi::ArrayBuffer::New(env, totalbytes);
	OSCaptureDesktop(buf.Data(), buf.ByteLength(), x, y, w, h);
	return Napi::Uint8Array::New(env, totalbytes, buf, 0, napi_uint8_clamped_array);
}

Napi::Value captureDesktopMulti(const Napi::CallbackInfo& info) {
	auto env = info.Env();
	auto obj = info[0].As<Napi::Object>();
	auto props = obj.GetPropertyNames();
	vector<CaptureRect> capts;
	int totalbytes = 0;
	auto ret = Napi::Object::New(env);
	for (int a = 0; a < props.Length(); a++) {
		auto key = props.Get(a);
		if (!key.IsString() || !obj.HasOwnProperty(key)) { continue; }
		CaptureRect capt;
		if (!ParseJsRect(obj.Get(props.Get(a)),&capt.rect)) { continue; }
		size_t size = capt.rect.width * capt.rect.height * 4;
		auto buffer = Napi::ArrayBuffer::New(env, size);
		capt.data = buffer.Data();
		capt.size = buffer.ByteLength();
		auto view = Napi::Uint8Array::New(env, size, buffer, 0, napi_uint8_clamped_array);
		ret.Set(key, view);
		capts.push_back(capt);
	}
	OSCaptureDesktopMulti(capts);
	return ret;
}

Napi::Value getProcessMainWindow(const Napi::CallbackInfo& info)
{
	uint32_t pid;
	if (!JsArgsUInt32(info, 0, &pid)) { return info.Env().Undefined(); }
	auto wnd = OSFindMainWindow(pid);
	return JsOSWindow::Create(info.Env(), wnd);
}

Napi::Value getProcessesByName(const Napi::CallbackInfo& info)
{
	string utf8str;
	if (!JsArgsString(info, 0, utf8str)) { return info.Env().Undefined(); }

	auto pids = OSGetProcessesByName(utf8str, NULL);
	auto ret = Napi::Array::New(info.Env(), pids.size());
	for (int i = 0; i < pids.size(); i++) { ret.Set(i, pids[i]); }
	return ret;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
	auto oswindowctr= JsOSWindow::GetClass(env);
	auto pinnedwndctr = JsPinnedWindow::GetClass(env);

	auto inst = new PluginInstance();
	inst->JsOSWindowCtr = new Napi::FunctionReference(Napi::Persistent(oswindowctr));
	inst->JsPinnedWindowCtr = new Napi::FunctionReference(Napi::Persistent(pinnedwndctr));
	//TODO need delete destructor to get rid of the mem again?
	env.SetInstanceData<>(inst);

	exports.Set("captureDesktop", Napi::Function::New(env, captureDesktop));
	exports.Set("getProcessMainWindow", Napi::Function::New(env, getProcessMainWindow));
	exports.Set("getProcessesByName", Napi::Function::New(env, getProcessesByName));
	exports.Set("captureDesktopMulti", Napi::Function::New(env, captureDesktopMulti));
	exports.Set("OSWindow", oswindowctr);
	return exports;
}

NODE_API_MODULE(hello, Init)