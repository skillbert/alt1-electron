#pragma once

class PinnedWindow :public WindowListener {
	OSWindow window;
	OSWindow pinparent;
	PinEdge edge;
	PinMode mode;
	int distleft;
	int disttop;
	int distright;
	int distbot;
	std::unique_ptr<OSWindowTracker> tracker;

public:
	void OnWindowMove(JSRectangle bounds, WindowDragPhase phase) {
		ParentMoved(bounds);
	}

	void OnWindowClose() {

	}

	void ParentMoved(JSRectangle parentBounds) {
		JSRectangle bounds = window.GetBounds();
			
		//determine where we should be
		if (edge & PinEdge::Left) {
			bounds.x = parentBounds.x + distleft;
			if (edge & PinEdge::Right) {
				bounds.width = parentBounds.width - distleft - distright;
			}
		}
		else if (edge & PinEdge::Right) {
			bounds.x = parentBounds.x + parentBounds.width - distright - bounds.width;
		}
		if (edge & PinEdge::Top) {
			bounds.y = parentBounds.y + disttop;
			if (edge & PinEdge::Bot) {
				bounds.height = parentBounds.height - disttop - distbot;
			}
		}
		else if (edge & PinEdge::Bot) {
			bounds.y = parentBounds.y + parentBounds.height- distbot- bounds.height;
		}

		//get active monitor bounds
		HMONITOR hmonitor = MonitorFromWindow(window.hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO monitor = {};
		monitor.cbSize = sizeof(MONITORINFO);
		GetMonitorInfoA(hmonitor, &monitor);

		//calculate viable area
		int minleft = min(monitor.rcWork.left, parentBounds.x);
		int mintop = min(monitor.rcWork.top, parentBounds.y);
		int maxright = max(monitor.rcWork.right, parentBounds.x + parentBounds.width);
		int maxbot = max(monitor.rcWork.bottom, parentBounds.y + parentBounds.height);

		//make sure the window is always inside either the parent window or the containing screen
		bounds.x = max(bounds.x, minleft);
		bounds.y = max(bounds.y, mintop);
		bounds.x = min(bounds.x, maxright - bounds.width);
		bounds.y = min(bounds.y, maxbot - bounds.height);

		window.SetBounds(bounds);
	}

	void UpdatePinAnchor() {
		JSRectangle selfbounds = window.GetBounds();
		JSRectangle parentbounds = pinparent.GetBounds();
		parentbounds.x++;

		distleft = selfbounds.x - parentbounds.x;
		disttop = selfbounds.y - parentbounds.y;
		distright = parentbounds.x + parentbounds.width - selfbounds.x - selfbounds.width;
		distbot = parentbounds.y + parentbounds.height - selfbounds.y - selfbounds.height;
		if (mode == PinMode::Auto) {
			//TODO find out how to do enum flags without cast???
			edge = static_cast<PinEdge>((disttop < distbot ? PinEdge::Top : PinEdge::Bot) | (distleft < distright ? PinEdge::Left : PinEdge::Right));
		}
		if (mode == PinMode::Cover) {
			edge = static_cast<PinEdge>(PinEdge::Left | PinEdge::Right | PinEdge::Top | PinEdge::Bot);
		}
	}

	static std::shared_ptr<PinnedWindow> NewPinning(OSWindow wnd, OSWindow parent,PinMode mode);
	static std::shared_ptr<PinnedWindow> FindPinning(OSWindow wnd);

private:
	PinnedWindow(OSWindow wnd,OSWindow parent,PinMode mode) {
		this->window = wnd;
		this->pinparent = parent;
		this->mode = mode;
		UpdatePinAnchor();

		//show behind parent, then show parent behind self (no way to show in front in winapi)
		SetWindowPos(this->window.hwnd, this->pinparent.hwnd, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		SetWindowPos(this->pinparent.hwnd, this->window.hwnd, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

		SetWindowLongPtr(wnd.hwnd, GWLP_HWNDPARENT, (uint64_t)parent.hwnd);
		tracker = OSNewWindowTracker(this->pinparent, this);
	}

};

class JsPinnedWindow :public Napi::ObjectWrap<JsPinnedWindow> {
public:
	JsPinnedWindow(const Napi::CallbackInfo& info) : Napi::ObjectWrap<JsPinnedWindow>(info) {}
	static Napi::Function GetClass(Napi::Env env) {
		return DefineClass(env, "PinnedWindow", {
			InstanceMethod("updatePinAnchor",&JsPinnedWindow::UpdatePinAnchor)
			});
	}
	static Napi::Object Create(Napi::Env env, std::shared_ptr<PinnedWindow> wnd) {
		auto obj=env.GetInstanceData<PluginInstance>()->JsPinnedWindowCtr->New({});
		Napi::ObjectWrap<JsPinnedWindow>::Unwrap(obj)->inst = wnd;
		return obj;
	}
private:
	std::shared_ptr<PinnedWindow> inst;

	void UpdatePinAnchor(const Napi::CallbackInfo& info) {
		inst->UpdatePinAnchor();
	}
};

//TODO move to instance
vector<std::shared_ptr<PinnedWindow>> pinnedWindows;


std::shared_ptr<PinnedWindow> PinnedWindow::NewPinning(OSWindow wnd, OSWindow parent, PinMode mode) {
	auto pin = std::shared_ptr<PinnedWindow>(new PinnedWindow(wnd, parent, mode));
	pinnedWindows.push_back(pin);
	return pin;
}
std::shared_ptr<PinnedWindow> PinnedWindow::FindPinning(OSWindow wnd) {
	for (auto it = pinnedWindows.begin(); it != pinnedWindows.end(); it++) {
		//TODO why can't i just compare these by value?
		if ((*it)->window.hwnd == wnd.hwnd) {
			return *it;
		}
	}
	return std::shared_ptr<PinnedWindow>(nullptr);
}

Napi::Value JsOSWindow::JsSetPinParent(const Napi::CallbackInfo& info) {
	OSWindow parent = JsArgsOSWindow(info[0]);
	string pinmodestr = info[1].As<Napi::String>().Utf8Value();

	PinMode pinmode;
	if (pinmodestr == "auto") { pinmode = PinMode::Auto; }
	else if (pinmodestr == "cover") { pinmode = PinMode::Cover; }
	else { Napi::TypeError::New(info.Env(), "invalid pinmode argument").ThrowAsJavaScriptException(); }
	auto pin = std::shared_ptr<PinnedWindow>(PinnedWindow::NewPinning(inst, parent, pinmode));
	return JsPinnedWindow::Create(info.Env(), pin);
}