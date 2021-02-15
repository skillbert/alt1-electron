import * as path from "path";
import * as fs from "fs";
import { Rectangle } from "./shared";
import { boundMethod } from "autobind-decorator";
import { TypedEmitter } from "./typedemitter";

//Copy the addon file so we can rebuild while alt1lite is already running
let addonpath = path.resolve(__dirname, "../build/Debug/");
let tmpfile = path.resolve(addonpath, "addon" + Math.floor(Math.random() * 1000) + ".node");
let origfile = path.resolve(addonpath, "addon.node");
fs.copyFileSync(origfile, tmpfile);

export const native = __non_webpack_require__(tmpfile) as {
	captureWindow: (wnd: BigInt, x: number, y: number, w: number, h: number) => Uint8ClampedArray,
	captureWindowMulti: <T extends { [key: string]: Rectangle | undefined | null }>(wnd: BigInt, rect: T) => { [key in keyof T]: Uint8ClampedArray },
	getProcessMainWindow: (pid: number) => BigInt,
	getProcessesByName: (name: string) => number[],

	getWindowBounds: (wnd: BigInt) => Rectangle,
	getClientBounds: (wnd: BigInt) => Rectangle,
	getWindowTitle: (wnd: BigInt) => string,
	setWindowBounds: (wnd: BigInt, x: number, y: number, w: number, h: number) => void,
	setWindowParent: (wnd: BigInt, parent: BigInt) => void,

	newWindowListener: (wnd: BigInt, ...args: windowEvents) => void,
	removeWindowListener: (wnd: BigInt, ...args: windowEvents) => void,

	test: (...arg: any) => any
}

type windowEvents =
	[type: "close", cb: () => any] |
	[type: "move", cb: (bounds: Rectangle, phase: "start" | "moving" | "end") => any];

export class OSWindow {
	handle: BigInt;
	constructor(handle: BigInt | Uint8Array) {
		if (handle instanceof Uint8Array) {
			this.handle = new BigUint64Array(handle.buffer, handle.byteOffset, handle.byteLength / BigUint64Array.BYTES_PER_ELEMENT)[0];
		} else if (typeof handle == "bigint") {
			this.handle = handle;
		}
	}
	getTitle() { return native.getWindowTitle(this.handle); }
	getBounds() { return native.getWindowBounds(this.handle); }
	getClientBounds() { return native.getClientBounds(this.handle); }
	setBounds(x: number, y: number, w: number, h: number) { return native.setWindowBounds(this.handle, x, y, w, h); }
	setParent(parent: OSWindow | null) { return native.setWindowParent(this.handle, parent ? parent.handle : BigInt(0)) }

	on(...[type, cb]: windowEvents) {
		native.newWindowListener(this.handle, type as any, cb);
	}
	removeListener(...[type, cb]: windowEvents) {
		native.removeWindowListener(this.handle, type as any, cb);
	}
}

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
	unpin() {
		native.setWindowParent(this.window.handle, BigInt(0));
		this.window.removeListener("move", this.onmove);
		this.window.removeListener("close", this.onclose);
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