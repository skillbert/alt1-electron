#pragma once

class PinnedWindow :public WindowListener {
	OSWindow window;
	OSWindow pinparent;
	PinEdge edge;
	int hordist;
	int verdist;
	std::unique_ptr<OSWindowTracker> tracker;

public:
	void OnWindowMove(JSRectangle bounds, WindowDragPhase phase) {
		ParentMoved(bounds);
	}

	void OnWindowClose() {

	}

	void ParentMoved(JSRectangle parentBounds) {
		JSRectangle selfbounds = window.GetBounds();

		//determine where we should end
		int newx, newy;
		if (edge == PinEdge::TopLeft || edge == PinEdge::TopRight) { newy = parentBounds.y + verdist; }
		else { newy = parentBounds.y + parentBounds.height - verdist - selfbounds.height; }
		if (edge == PinEdge::TopLeft || edge == PinEdge::BotLeft) { newx = parentBounds.x + hordist; }
		else { newx = parentBounds.x + parentBounds.width - hordist - selfbounds.width; }

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
		newx = max(newx, minleft);
		newy = max(newy, mintop);
		newx = min(newx, maxright - selfbounds.width);
		newy = min(newy, maxbot - selfbounds.height);

		window.SetBounds(JSRectangle(newx, newy, selfbounds.width, selfbounds.height));
	}

	void UpdatePinAnchor() {
		JSRectangle selfbounds = window.GetBounds();
		JSRectangle parentbounds = pinparent.GetBounds();

		int left = selfbounds.x - parentbounds.x;
		int top = selfbounds.y - parentbounds.y;
		int right = parentbounds.x + parentbounds.width - selfbounds.x - selfbounds.width;
		int bottom = parentbounds.y + parentbounds.height - selfbounds.y - selfbounds.height;
		hordist = min(left, right);
		verdist = min(top, bottom);
		if (left < right) {
			edge = (top < bottom ? PinEdge::TopLeft : PinEdge::BotLeft);
		}
		else {
			edge = (top < bottom ? PinEdge::TopRight : PinEdge::BotRight);
		}
	}

	static std::shared_ptr<PinnedWindow> NewPinning(OSWindow wnd, OSWindow parent);
	static std::shared_ptr<PinnedWindow> FindPinning(OSWindow wnd);

private:
	PinnedWindow(OSWindow wnd,OSWindow parent) {
		this->window = wnd;
		this->pinparent = parent;
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


std::shared_ptr<PinnedWindow> PinnedWindow::NewPinning(OSWindow wnd, OSWindow parent) {
	auto pin = std::shared_ptr<PinnedWindow>(new PinnedWindow(wnd, parent));
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
	OSWindow parent;
	JsArgsOSWindow(info, 0, &parent);
	auto pin = std::shared_ptr<PinnedWindow>(PinnedWindow::NewPinning(inst, parent));
	return JsPinnedWindow::Create(info.Env(), pin);
}