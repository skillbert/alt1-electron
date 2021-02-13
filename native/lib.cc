

//todo if windows
#include "oswin.h"
#include "pinnedwindow.h"


Napi::Value captureDesktop(const Napi::CallbackInfo& info) {
	auto env = info.Env();
	int x = info[0].As<Napi::Number>().Int32Value();
	int y = info[1].As<Napi::Number>().Int32Value();
	int w = info[2].As<Napi::Number>().Int32Value();
	int h = info[3].As<Napi::Number>().Int32Value();

	if (w <= 0 || h <= 0 || w > 1e4 || h > 1e4)
	{
		Napi::TypeError::New(env, "invalid capture size").ThrowAsJavaScriptException();
	}

	int totalbytes = w * h * 4;
	auto buf = Napi::ArrayBuffer::New(env, totalbytes);
	OSCaptureDesktop(buf.Data(), buf.ByteLength(), x, y, w, h);
	auto arr = Napi::Uint8Array::New(env, totalbytes, buf, 0, napi_uint8_clamped_array);
	//TODO what is the warning "do not slice"
	return arr;
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
		auto val = obj.Get(key);
		if (val.IsNull() || val.IsUndefined()) { continue; }
		CaptureRect capt;
		capt.rect = ParseJsRect(val);
		size_t size = (size_t)capt.rect.width * capt.rect.height * 4;
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

Napi::Value getProcessMainWindow(const Napi::CallbackInfo& info) {
	uint32_t pid = info[0].As<Napi::Number>().Uint32Value();
	auto wnd = OSFindMainWindow(pid);
	return JsOSWindow::Create(info.Env(), wnd);
}

Napi::Value getProcessesByName(const Napi::CallbackInfo& info) {
	string name = info[0].As<Napi::String>().Utf8Value();
	auto pids = OSGetProcessesByName(name, NULL);
	auto ret = Napi::Array::New(info.Env(), pids.size());
	for (int i = 0; i < pids.size(); i++) { ret.Set(i, pids[i]); }
	return ret;
}

//TODO remove
Napi::Value test(const Napi::CallbackInfo& info) {
	bool loss;
	auto val = info[0].As<Napi::BigInt>().Uint64Value(&loss);
	return Napi::Boolean::New(info.Env(), loss);
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


	exports.Set("test", Napi::Function::New(env, test));
	return exports;
}

NODE_API_MODULE(hello, Init)