import { app, BrowserWindow } from "electron";
import * as electron from "electron";
import * as path from "path";
import { delay } from "./lib";
import { OSWindow, native, OSWindowPin, OSNullWindow } from "./native";
import { OverlayCommand } from "./shared";
import { TypedEmitter } from "./typedemitter";
import { boundMethod } from "autobind-decorator";
import { AppPermission, settings } from "./settings";
import { openApp, managedWindows, selectAppContexts } from "./main";
import { Alt1EventType, ImgRef, ImgRefData, PointLike, Rect, RectLike } from "@alt1/base";
import { readAnything } from "./readers/alt1reader";
import RightClickReader from "./readers/rightclick";


export var rsInstances: RsInstance[] = [];

export function initRsInstanceTracking() {
	detectInstances();
	// I don't think we need to do any tracking as long as detectInstances() is called appropriately
};

export function detectInstances() {
	let handles = native.getRsHandles();

	// Make a list of currently-tracked instances that were not detected this time, for removal
	// Note: this variable is not inlined because iterating a list while modifying it would not behave as expected
	const removedInstances = rsInstances.filter((x) => !handles.includes(x.window.handle));

	// Close and remove each one
	for (const instance of removedInstances) {
		instance.close();
		rsInstances = rsInstances.filter((x) => x.window != instance.window);
	}

	// Add any new ones
	handles.filter((x) => !rsInstances.map((x) => x.window.handle).includes(x)).map((x) => new RsInstance(new OSWindow(x)));
}

export function getRsInstanceFromWnd(wnd: OSWindow) {
	let rsinst = rsInstances.find(rs => rs.window.handle == wnd.handle);
	if (rsinst) { return rsinst; }
	if (!rsinst) {
		//TODO check popups as well
		let appwnd = managedWindows.find(w => w.nativeWindow.handle == wnd.handle);
		if (appwnd) { return appwnd.rsClient; }
	}
	return null;
}

type RsInstanceEvents = {
	close: []
}


//TODO this class is just straight up weird
class ActiveRightclick {
	reader: RightClickReader;
	inst: RsInstance;
	interval: number;
	imgref: ImgRef;
	clientRect: RectLike;
	constructor(client: RsInstance, reader: RightClickReader, imgref: ImgRef) {
		this.reader = reader;
		this.inst = client;
		this.imgref = imgref;
		this.clientRect = new Rect(reader.pos!.x + imgref.x, reader.pos!.y + imgref.y, reader.pos!.width, reader.pos!.height);

		this.interval = setInterval(this.check, 100) as any;
		let screentopleft = this.inst.clientToScreen({ x: this.clientRect.x, y: this.clientRect.y });
		let screenbotright = this.inst.clientToScreen({ x: this.clientRect.x + this.clientRect.width, y: this.clientRect.y + this.clientRect.height });
		for (let wnd of managedWindows) {
			if (wnd.rsClient != this.inst) { continue; }
			let wndbounds = wnd.nativeWindow.getClientBounds();
			let rect = {
				x: screentopleft.x - wndbounds.x,
				y: screentopleft.y - wndbounds.y,
				width: screenbotright.x - screentopleft.x,
				height: screenbotright.y - screentopleft.y
			};
			wnd.window.webContents.send("rightclick", rect);
		}
		this.inst.activeRightclick = this;
	}

	close() {
		for (let wnd of managedWindows) {
			if (wnd.rsClient != this.inst) { continue; }
			wnd.window.webContents.send("rightclick", null);
		}
		clearInterval(this.interval);
		this.inst.activeRightclick = null;
	}

	@boundMethod
	check() {
		let screenpos = electron.screen.getCursorScreenPoint();
		let mouse = this.inst.screenToClient(screenpos);
		let pos = this.clientRect;
		const margin = 10;
		if (mouse.x < pos.x - margin || mouse.y < pos.y - margin || mouse.x > pos.x + pos.width + margin || mouse.y > pos.y + pos.height + margin) {
			this.close();
		}
	}
}

export class RsInstance extends TypedEmitter<RsInstanceEvents>{
	window: OSWindow;
	overlayWindow: { browser: BrowserWindow, nativewnd: OSWindow, pin: OSWindowPin | null, stalledOverlay: { frameid: number, cmd: OverlayCommand[] }[] } | null;
	activeRightclick: ActiveRightclick | null = null;
	isActive = false;
	lastBlurTime = 0;

	constructor(rswindow: OSWindow) {
		super();
		this.window = rswindow;
		this.window.on("close", this.close);
		this.window.on("click", this.clientClicked);
		this.overlayWindow = null;

		for (let app of settings.bookmarks) {
			if (app.wasOpen) {
				app.wasOpen = false;
				openApp(app);
			}
		}

		rsInstances.push(this);
		console.log(`new rs client tracked with handle: ${this.window.handle}`);
	}

	@boundMethod
	close() {
		rsInstances.splice(rsInstances.indexOf(this), 1);
		this.window.removeListener("close", this.close);
		this.emit("close");
		console.log(`stopped tracking rs client with handle: ${this.window.handle}`);
	}

	emitAppEvent<T extends keyof Alt1EventType>(permission: AppPermission | "", type: T, event: Alt1EventType[T]) {
		for (let context of selectAppContexts(this, permission)) {
			context.send("appevent", type, event);
		}
	}

	@boundMethod
	async clientClicked() {
		if (this.activeRightclick) {
			this.activeRightclick.close();
		}
		//TODO actually check if it is a rightclick
		if (true) {
			//need to wait for 2 frames to get rendered (doublebuffered)
			await delay(2 * 50);
			let mousepos = this.screenToClient(electron.screen.getCursorScreenPoint());
			let captrect = new Rect(mousepos.x - 300, mousepos.y - 300, 600, 600);
			captrect.intersect({ x: 0, y: 0, ...this.getClientSize() });
			if (captrect.width <= 0 || captrect.height <= 0) {
				console.log("tried to capture 0 size area around mouse click");
				return;
			}
			let capt = this.capture(captrect);
			let reader = new RightClickReader();
			let img = new ImgRefData(capt, 0, 0);
			if (reader.find(img)) {
				let pos = reader.pos!;
				new ActiveRightclick(this, reader, new ImgRefData(capt, captrect.x, captrect.y));
				this.emitAppEvent("", "menudetected", {
					eventName: "menudetected",
					rectangle: {
						x: pos.x + captrect.x,
						y: pos.y + captrect.y,
						width: pos.width,
						height: pos.height
					}
				});
			}
		}
	}

	setActive(active: boolean) {
		if (active != this.isActive) {
			this.isActive = active;
			if (this.isActive) {
				this.emitAppEvent("", "rsfocus", { eventName: "rsfocus" });
			} else {
				this.emitAppEvent("", "rsblur", { eventName: "rsblur" });
			}
		}
	}

	screenToClient(p: PointLike) {
		let rsrect = this.window.getClientBounds();
		//TODO add scaling
		return { x: p.x - rsrect.x, y: p.y - rsrect.y };
	}

	clientToScreen(p: PointLike) {
		let rsrect = this.window.getClientBounds();
		//TODO add scaling
		return { x: p.x + rsrect.x, y: p.y + rsrect.y };
	}

	getClientSize() {
		//TODO this doesn't account for scaling
		let rect = this.window.getClientBounds();
		return { width: rect.width, height: rect.height };
	}

	capture(rect: RectLike) {
		let capt = native.captureWindowMulti(this.window.handle, settings.captureMode, { main: rect });
		return new ImageData(capt.main, rect.width, rect.height);
	}

	alt1Pressed() {
		let mousescreen = electron.screen.getCursorScreenPoint();
		let mousepos = this.screenToClient(mousescreen);
		let captrect = new Rect(mousepos.x - 300, mousepos.y - 300, 600, 600);
		captrect.intersect({ x: 0, y: 0, ...this.getClientSize() });
		if (!captrect.containsPoint(mousepos.x, mousepos.y)) { throw new Error("alt+1 pressed outside client"); }
		let img = this.capture(captrect);
		let res = readAnything(img, mousepos.x - captrect.x, mousepos.y - captrect.y);
		if (res?.type == "text") {
			let str = res.line.text;
			console.log("text " + res.font + ": " + str);
			//TODO grab these from c# alt1

		} else if (res?.type == "rightclick") {
			console.log("rightclick: " + res.line.text);
		}
		else {
			console.log("no text found under cursor")
		}
		this.emitAppEvent("", "alt1pressed", {
			eventName: "alt1pressed",
			text: res?.line.text || "",
			rsLinked: true,//event is no emited in new api if this is not true
			x: mousepos.x, y: mousepos.y,
			mouseAbs: mousescreen,
			mouseRs: mousepos
		});
	}

	overlayCommands(frameid: number, commands: OverlayCommand[]) {
		if (!this.overlayWindow) {
			console.log("opening overlay");
			let bounds = this.window.getClientBounds();
			let browser = new BrowserWindow({
				webPreferences: { nodeIntegration: true, contextIsolation: false },
				frame: false,
				transparent: true,
				x: bounds.x,
				y: bounds.y,
				width: bounds.width,
				height: bounds.height,
				show: false,
				//resizable: false,
				movable: false,
				skipTaskbar: true,
				focusable: false
			});

			let nativewnd = new OSWindow(browser.getNativeWindowHandle());
			let pin: OSWindowPin | null = null;
			if (process.platform != "linux") {
				// pinning this on linux would make it non-transparent; electron is trashware 🤡
				pin = new OSWindowPin(nativewnd, this.window, "cover");
			}
			browser.loadFile(path.resolve(__dirname, "overlayframe/index.html"));
			browser.once("ready-to-show", () => {
				browser.show();
			});
			browser.webContents.once("dom-ready", e => {
				for (let stalled of this.overlayWindow!.stalledOverlay) {
					browser.webContents.send("overlay", stalled.frameid, stalled.cmd);
				}
			});
			browser.on("closed", e => {
				this.overlayWindow = null;
				console.log("overlay closed");
			});
			browser.setIgnoreMouseEvents(true);
			this.overlayWindow = { browser, nativewnd, pin, stalledOverlay: [{ frameid: frameid, cmd: commands }] };
		} else {
			this.overlayWindow.browser.webContents.send("overlay", frameid, commands);
		}
	}
}
