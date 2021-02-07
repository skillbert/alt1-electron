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
		JSRectangle selfbounds = OSGetWindowBounds(window);

		//determine where we should end
		int newx, newy;
		if (edge == PinEdge::TopLeft || edge == PinEdge::TopRight) { newy = parentBounds.y + verdist; }
		else { newy = parentBounds.y + parentBounds.height - verdist - selfbounds.height; }
		if (edge == PinEdge::TopLeft || edge == PinEdge::BotLeft) { newx = parentBounds.x + hordist; }
		else { newx = parentBounds.x + parentBounds.width - hordist - selfbounds.width; }

		//get active monitor bounds
		HMONITOR hmonitor = MonitorFromWindow(window.GetHwnd(), MONITOR_DEFAULTTONEAREST);
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

		OSSetWindowBounds(window, MakeJSRect(newx, newy, selfbounds.width, selfbounds.height));
	}

	void UpdatePinAnchor() {
		JSRectangle selfbounds = OSGetWindowBounds(window);
		JSRectangle parentbounds = OSGetWindowBounds(pinparent);

		int left = selfbounds.x - parentbounds.x;
		int top = selfbounds.y - parentbounds.y;
		int right = parentbounds.x + parentbounds.width - selfbounds.x - selfbounds.width;
		int bottom = parentbounds.y + parentbounds.height - selfbounds.y - selfbounds.height;
		hordist = min(left, right);
		verdist = min(top, bottom);
		if (left < right) {
			edge = (top < bottom ? PinEdge::TopLeft : PinEdge::TopLeft);
		}
		else {
			edge = (top < bottom ? PinEdge::TopRight : PinEdge::BotRight);
		}
	}

	PinnedWindow(OSWindow wnd,OSWindow parent) {
		this->window = wnd;
		this->pinparent = parent;
		UpdatePinAnchor();
		tracker = OSNewWindowTracker(this->pinparent, this);
	}

	static std::shared_ptr<PinnedWindow> FindPinning(OSWindow wnd) {
		for (const auto &pin:pinnedWindows) {
			//TODO why can't i just compare these by value?
			if (pin->window.GetRaw() == wnd.GetRaw()) {
				return pin;
			}
		}
		return std::shared_ptr<PinnedWindow>(nullptr);
	}
};

vector<std::shared_ptr<PinnedWindow>> pinnedWindows;

