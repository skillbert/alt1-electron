import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import fetch from "node-fetch";
import { dialog, Menu, MenuItem, Tray } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs, handleSchemeCommand } from "./schemehandler";
import { readJsonWithBOM, relPath, rsClientExe, sameDomainResolve, schemestring, weborigin } from "./lib";
import { identifyApp } from "./appconfig";
import { OSWindow, native, OSWindowPin, OSNullWindow, getActiveWindow } from "./native";
import { OverlayCommand } from "./shared";
import { EventEmitter } from "events";
import { TypedEmitter } from "./typedemitter";
import { boundMethod } from "autobind-decorator";
import { settings } from "./settings";
import { openApp, managedWindows } from "./main";
import { ImgRefData, Rect, RectLike } from "@alt1/base";
import { readAnything } from "./readers/alt1reader";
import RightClickReader from "./readers/rightclick";


export var rsInstances: RsInstance[] = [];

function init() {
	detectInstances();
	OSNullWindow.on("show", windowCreated);
};
setImmediate(init);

function windowCreated(handle: BigInt) {
	if (handle == BigInt(0)) { return; }
	let wnd = new OSWindow(handle);
	let pid = native.getWindowPid(wnd.handle);
	if (!rsInstances.find(q => q.pid == pid)) {
		let processname = native.getProcessName(pid);
		let title = wnd.getTitle();
		if (title.startsWith("RuneScape") && processname == rsClientExe) {
			new RsInstance(pid);
		}
	}
}

export function detectInstances() {
	let pids = native.getProcessesByName(rsClientExe);
	for (let inst of rsInstances) {
		if (pids.indexOf(inst.pid) == -1) {
			inst.close();
		}
	}
	for (let pid of pids) {
		let inst = rsInstances.find(q => q.pid == pid);
		if (!inst) {
			new RsInstance(pid);
		}
	}
}

export function getActiveInstance() {
	let wnd = getActiveWindow();
	let rsinst = rsInstances.find(rs => rs.window.handle == wnd.handle);
	if (rsinst) { return rsinst; }
	if (!rsinst) {
		let appwnd = managedWindows.find(w => w.nativeWindow.handle == wnd.handle);
		if (appwnd) { return appwnd.rsClient; }
	}
	return null;
}

type RsInstanceEvents = {
	close: []
}

export class RsInstance extends TypedEmitter<RsInstanceEvents>{
	pid: number;
	window: OSWindow;
	overlayWindow: { browser: BrowserWindow, nativewnd: OSWindow, pin: OSWindowPin, stalledOverlay: { frameid: number, cmd: OverlayCommand[] }[] } | null;

	constructor(pid: number) {
		super();
		this.pid = pid;
		this.window = new OSWindow(native.getProcessMainWindow(pid));
		this.window.on("close", this.close);
		this.overlayWindow = null;

		for (let app of settings.bookmarks) {
			if (app.wasOpen) {
				app.wasOpen = false;
				openApp(app);
			}
		}

		rsInstances.push(this);
		console.log(`new rs client tracked with pid: ${pid}`);
	}

	@boundMethod
	close() {
		rsInstances.splice(rsInstances.indexOf(this), 1);
		this.window.removeListener("close", this.close);
		this.emit("close");
		console.log(`stopped tracking rs client with pid: ${this.pid}`);
	}

	captureCursorArea(relrect: RectLike) {
		let rsrect = this.window.getClientBounds();
		//TODO map mouse correctly when scaled
		let mouseabs = electron.screen.getCursorScreenPoint();
		let mousex = mouseabs.x - rsrect.x;
		let mousey = mouseabs.y - rsrect.y;

		//TODO better sizing
		let captrect = new Rect(mousex + relrect.x, mousey + relrect.y, relrect.width, relrect.height);
		captrect.intersect(new Rect(0, 0, rsrect.width, rsrect.height));
		let capt = native.captureWindow(this.window.handle, captrect.x, captrect.y, captrect.width, captrect.height);
		let img = new ImageData(capt, captrect.width, captrect.height);
		return { img, rect: new Rect(captrect.x - mousex, captrect.y - mousey, captrect.width, captrect.height) };
	}

	alt1Pressed() {
		let capt = this.captureCursorArea(new Rect(-300, -300, 600, 600));
		if (!capt.rect.containsPoint(0, 0)) { throw new Error("alt+1 pressed outside client"); }
		let res = readAnything(capt.img, -capt.rect.x, -capt.rect.y);
		if (res?.type == "text") {
			let str = res.line.text;
			console.log("text " + res.font + ": " + str);
			//TODO grab these from c# alt1
			if (res.font == "rightclick") {

			}
		} else {
			console.log("no text found under cursor")
		}
		//TODO run alt1pressed event
	}

	rightClicked() {
		let capt = this.captureCursorArea(new Rect(-300, -300, 600, 600));
		let reader = new RightClickReader();
		let img = new ImgRefData(capt.img, 0, 0);
		if (reader.find(img)) {
			//TODO notify and hide all overlapping apps

			//TODO run rightclicked event
		}
	}

	overlayCommands(frameid: number, commands: OverlayCommand[]) {
		if (!this.overlayWindow) {
			console.log("opening overlay");
			let bounds = this.window.getClientBounds();
			let browser = new BrowserWindow({
				webPreferences: { nodeIntegration: true, webviewTag: true, enableRemoteModule: true },
				frame: false,
				transparent: true,
				x: bounds.x,
				y: bounds.y,
				width: bounds.width,
				height: bounds.height,
				show: false,
				//resizable: false,
				movable: false,
				skipTaskbar: true
			});

			let nativewnd = new OSWindow(browser.getNativeWindowHandle());
			let pin = new OSWindowPin(nativewnd, this.window, "cover");
			browser.loadFile(path.resolve(__dirname, "overlayframe/index.html"));
			//browser.webContents.openDevTools();
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