import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import fetch from "node-fetch";
import { dialog, Menu, MenuItem, Tray } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs, handleSchemeCommand } from "./schemehandler";
import { readJsonWithBOM, relPath, rsClientExe, sameDomainResolve, schemestring, UserError, weborigin } from "./lib";
import { identifyApp } from "./appconfig";
import { OSWindow, native, OSWindowPin, OSNullWindow } from "./native";
import { OverlayCommand } from "./shared";
import { EventEmitter } from "events";
import { TypedEmitter } from "./typedemitter";
import { boundMethod } from "autobind-decorator";
import { settings } from "./settings";
import { openApp } from "./main";


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