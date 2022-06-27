import * as path from "path";
import * as fs from "fs";
import { Rectangle } from "./shared";
import { boundMethod } from "autobind-decorator";
import { TypedEmitter } from "./typedemitter";
import { PinRect } from "./settings";

export type CaptureMode = "desktop" | "window" | "opengl";

export var native: {
	captureWindowMulti: <T extends { [key: string]: Rectangle | undefined | null }>(wnd: BigInt, mode: CaptureMode, rect: T) => { [key in keyof T]: Uint8ClampedArray },
	getRsHandles: () => BigInt[],
	getActiveWindow: () => BigInt,
	getWindowBounds: (wnd: BigInt) => Rectangle,
	getClientBounds: (wnd: BigInt) => Rectangle,
	getWindowTitle: (wnd: BigInt) => string,
	setWindowBounds: (wnd: BigInt, x: number, y: number, w: number, h: number) => void,
	setWindowParent: (wnd: BigInt, parent: BigInt) => void,

	newWindowListener: <T extends keyof windowEvents>(wnd: BigInt, type: T, cb: windowEvents[T]) => void,
	removeWindowListener: <T extends keyof windowEvents>(wnd: BigInt, type: T, cb: windowEvents[T]) => void,

	test: (...arg: any) => any
};
reloadAddon();

//(Re)loads the native code, this gives all kinds of mem leaks and other trouble if called more than once, only do so for debugging
export function reloadAddon() {
	//TODO fix hardcoded build path
	let addonpath = path.resolve(__dirname, "../build/Debug/");

	//Copy the addon file so we can rebuild while alt1lite is already running
	if (process.env.NODE_ENV === "development") {
		let tmpfile = path.resolve(addonpath, "addon" + Math.floor(Math.random() * 1000) + ".node");
		let origfile = path.resolve(addonpath, "addon.node");
		fs.copyFileSync(origfile, tmpfile);
		addonpath = tmpfile;
	}
	native = __non_webpack_require__(addonpath);
}

type windowEvents = {
	close: () => any,
	move: (bounds: Rectangle, phase: "start" | "moving" | "end") => any,
	show: (wnd: BigInt, event: number) => any,
	click: () => any
};

export function getActiveWindow() {
	return new OSWindow(native.getActiveWindow());
}

export class OSWindow {
	handle: BigInt;
	constructor(handle: BigInt | Buffer) {
		if (handle instanceof Buffer) {
			if (handle.byteLength == 8) { this.handle = handle.readBigUInt64LE(); }
			else if (handle.byteLength == 4) { this.handle = BigInt(handle.readUInt32LE()); }
			else { throw new Error("unexpected handle size"); }
		} else if (typeof handle == "bigint") {
			this.handle = handle;
		}
	}
	getTitle() { return native.getWindowTitle(this.handle); }
	getBounds() { return native.getWindowBounds(this.handle); }
	getClientBounds() { return native.getClientBounds(this.handle); }
	setBounds(x: number, y: number, w: number, h: number) { return native.setWindowBounds(this.handle, x, y, w, h); }
	setParent(parent: OSWindow | null) { return native.setWindowParent(this.handle, parent ? parent.handle : BigInt(0)) }

	on<T extends keyof windowEvents>(type: T, cb: windowEvents[T]) {
		native.newWindowListener(this.handle, type, cb);
	}
	removeListener<T extends keyof windowEvents>(type: T, cb: windowEvents[T]) {
		native.removeWindowListener(this.handle, type, cb);
	}
}

//can mean different things depending on context
//usually means the desktop or "any" window
export const OSNullWindow = new OSWindow(BigInt(0));

type OSWindowPinEvents = {
	close: [],
	unpin: [],
	moved: []
};

export class OSWindowPin extends TypedEmitter<OSWindowPinEvents>{
	window: OSWindow;
	parent: OSWindow;
	pinhor: "left" | "right";
	pinver: "top" | "bot";
	wndhordist = 0;
	wndverdist = 0;
	wndwidth = 0;
	wndheight = 0;
	dockmode: "cover" | "auto";
	constructor(window: OSWindow, parent: OSWindow, dockmode: "cover" | "auto") {
		super();
		this.window = window;
		this.parent = parent;
		this.dockmode = dockmode;
		this.pinhor = "left";
		this.pinver = "top";
		this.updateDocking();
		native.setWindowParent(window.handle, parent.handle);
		this.parent.on("move", this.onmove);
		this.parent.on("close", this.onclose);
	}
	setPinRect(rect: PinRect) {
		let isleft = rect.pinning.includes("left");
		this.pinhor = isleft ? "left" : "right";
		this.wndhordist = isleft ? rect.left : rect.right;
		this.wndwidth = rect.width;
		let istop = rect.pinning.includes("top");
		this.pinver = istop ? "top" : "bot";
		this.wndverdist = istop ? rect.top : rect.bot;
		this.wndheight = rect.height;
		this.synchPosition();
	}
	getPinRect() {
		let r: PinRect = {
			left: this.pinhor == "left" ? this.wndhordist : 0,
			right: this.pinhor == "right" ? this.wndhordist : 0,
			top: this.pinver == "top" ? this.wndverdist : 0,
			bot: this.pinver == "bot" ? this.wndverdist : 0,
			height: this.wndheight,
			width: this.wndwidth,
			pinning: [this.pinver, this.pinhor]
		};
		return r;
	}
	unpin() {
		native.setWindowParent(this.window.handle, BigInt(0));
		this.parent.removeListener("move", this.onmove);
		this.parent.removeListener("close", this.onclose);
		this.emit("unpin");
	}
	updateDocking() {
		if (this.dockmode == "auto") {
			let parentbounds = this.parent.getBounds();
			let bounds = this.window.getBounds();

			let left = bounds.x - parentbounds.x;
			let top = bounds.y - parentbounds.y;
			let right = parentbounds.x + parentbounds.width - bounds.x - bounds.width;
			let bot = parentbounds.y + parentbounds.height - bounds.y - bounds.height;
			this.pinhor = (left < right ? "left" : "right");
			this.pinver = (top < bot ? "top" : "bot");
			this.wndhordist = Math.min(left, right);
			this.wndverdist = Math.min(top, bot);
			this.wndwidth = bounds.width;
			this.wndheight = bounds.height;
		}
	}
	synchPosition(parentbounds?: Rectangle) {
		if (this.dockmode == "auto") {
			parentbounds = parentbounds || this.parent.getBounds();
			let x = (this.pinhor == "left" ? parentbounds.x + this.wndhordist : parentbounds.x + parentbounds.width - this.wndhordist - this.wndwidth);
			let y = (this.pinver == "top" ? parentbounds.y + this.wndverdist : parentbounds.y + parentbounds.height - this.wndverdist - this.wndheight);
			this.window.setBounds(x, y, this.wndwidth, this.wndheight);
		}
		if (this.dockmode == "cover") {
			let bounds = this.parent.getClientBounds();
			this.window.setBounds(bounds.x, bounds.y, bounds.width, bounds.height);
		}
	}
	@boundMethod
	onmove(bounds: Rectangle, phase: "start" | "moving" | "end") {
		this.synchPosition(bounds);
		this.emit("moved");
	}
	@boundMethod
	onclose() {
		this.unpin();
		this.emit("close");
		this.removeAllListeners();
	}
}
