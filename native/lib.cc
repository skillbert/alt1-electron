

//todo if windows
#include "oswin.h"
#include "pinnedwindow.h"


NAN_METHOD(captureDesktop)
{
	auto ctx = info.GetIsolate()->GetCurrentContext();

	int x, y, w, h;
	if (!JsArgsInt32(info, 0, &x) || !JsArgsInt32(info, 1, &y) || !JsArgsInt32(info, 2, &w) || !JsArgsInt32(info, 3, &h)) {
		return;
	}

	if (w <= 0 || h <= 0 || w > 1e4 || h > 1e4)
	{
		Nan::ThrowTypeError("invalid capture size");
		return;
	}

	int totalbytes = w * h * 4;
	vector<byte> pixeldata;
	pixeldata.resize(totalbytes);
	auto buf = v8::ArrayBuffer::New(info.GetIsolate(), totalbytes);
	auto view = v8::Uint8ClampedArray::New(buf, 0, totalbytes);
	auto bufcontent = buf->GetContents();
	OSCaptureDesktop(bufcontent.Data(), bufcontent.ByteLength(), x, y, w, h);
	info.GetReturnValue().Set(view);
}

NAN_METHOD(getProcessMainWindow)
{
	auto ctx = info.GetIsolate()->GetCurrentContext();

	uint32_t pid;

	if (!info[0]->Uint32Value(ctx).To(&pid))
	{
		Nan::ThrowTypeError("Expected one uin32");
		return;
	}
	OSWindow wnd(OSFindMainWindow(pid));
	info.GetReturnValue().Set(wnd.GetJsPointer(info.GetIsolate()));
}

NAN_METHOD(getProcessesByName)
{
	string utf8str;
	if (!JsArgsString(info, 0, utf8str)) {
		return;
	}
	//TODO this gives garbled text in non ascii strings (shouldn't matter anyway as we are matching proc names)
	wstring procname = std::wstring(utf8str.begin(), utf8str.end());
	auto pids = OSGetProcessesByName(procname, NULL);
	auto ret = Nan::New<v8::Array>(pids.size());
	for (int i = 0; i < pids.size(); i++)
	{
		Nan::Set(ret, i, Nan::New((double)pids[i]));
	}
	info.GetReturnValue().Set(ret);
}

NAN_METHOD(setPinParent) {
	OSWindow wnd, parent;
	if (!JsArgsOSWindow(info, 0, &wnd) || !JsArgsOSWindow(info, 1, &parent)) {
		return;
	}
	SetWindowLongPtr(wnd.GetHwnd(), GWLP_HWNDPARENT, (uint64_t)parent.GetHwnd());
	//TODO don't lose this ref
	new PinnedWindow(wnd, parent);
}

NAN_METHOD(getWindowMeta)
{
	OSWindow wnd;
	if (!JsArgsOSWindow(info, 0, &wnd))
	{
		return;
	}

	if (!OSIsWindow(wnd))
	{
		info.GetReturnValue().SetNull();
		return;
	}

	JSRectangle wndrect = OSGetWindowBounds(wnd);
	string wndTitle = OSGetWindowTitle(wnd);

	auto obj = Nan::New<v8::Object>();
	Nan::Set(obj, v8string("x"), Nan::New(wndrect.x));
	Nan::Set(obj, v8string("y"), Nan::New(wndrect.y));
	Nan::Set(obj, v8string("width"), Nan::New(wndrect.width));
	Nan::Set(obj, v8string("height"), Nan::New(wndrect.height));
	Nan::Set(obj, v8string("title"), Nan::New(wndTitle.c_str()).ToLocalChecked());
	info.GetReturnValue().Set(obj);
}

NAN_MODULE_INIT(Initialize) {
	NAN_EXPORT(target, captureDesktop);
	NAN_EXPORT(target, getProcessMainWindow);
	NAN_EXPORT(target, getProcessesByName);
	NAN_EXPORT(target, getWindowMeta);
	NAN_EXPORT(target, setPinParent);
}

NODE_MODULE(module_name, Initialize)