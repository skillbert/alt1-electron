import { app, BrowserWindow, globalShortcut, ipcMain, WebContents } from "electron";
import * as electron from "electron";
import * as path from "path";
import fetch from "node-fetch";
import { dialog, Menu, MenuItem, Tray } from "electron/main";
import { MenuItemConstructorOptions, nativeImage } from "electron/common";
import { handleSchemeArgs, handleSchemeCommand } from "./schemehandler";
import { readJsonWithBOM, relPath, rsClientExe, sameDomainResolve, schemestring, UserError, weborigin } from "./lib";
import { InstalledApp, identifyApp } from "./appconfig";
import { getProcessesByName, getProcessMainWindow, OSWindow, OSWindowPin } from "./native";
import { OverlayCommand } from "./shared";



export var rsInstances: RsInstance[] = [];

export function detectInstances() {
	let pids = getProcessesByName(rsClientExe);
	for (let pid of pids) {
		let inst = rsInstances.find(q => q.pid == pid);
		if (!inst) {
			new RsInstance(pid);
		}
	}
}

export class RsInstance {
	pid: number;
	window: OSWindow;
	overlayWindow: { browser: BrowserWindow, nativewnd: OSWindow, pin: OSWindowPin, stalledOverlay: { frameid: number, cmd: OverlayCommand[] }[] } | null;

	constructor(pid: number) {
		this.pid = pid;
		this.window = getProcessMainWindow(pid);
		this.overlayWindow = null;

		rsInstances.push(this);
	}

	close() {
		rsInstances.splice(rsInstances.indexOf(this), 1);
	}

	overlayCommands(frameid: number, commands: OverlayCommand[]) {
		if (!this.overlayWindow) {
			let bounds = this.window.getClientBounds();
			let browser = new BrowserWindow({
				webPreferences: { nodeIntegration: true, webviewTag: true, enableRemoteModule: true },
				frame: false,
				transparent: true,
				backgroundColor: "transparent",
				x: bounds.x,
				y: bounds.y,
				width: bounds.width,
				height: bounds.height,
				show: false,
				resizable:false,
				movable:false
			});

			let nativewnd = new OSWindow(browser.getNativeWindowHandle());
			let pin = nativewnd.setPinParent(this.window);
			browser.loadFile(path.resolve(__dirname, "overlayframe/index.html"));
			browser.webContents.openDevTools();
			browser.once("ready-to-show", () => {
				browser.show();
			});
			browser.webContents.on("dom-ready", e => {
				for (let stalled of this.overlayWindow!.stalledOverlay) {
					browser.webContents.send("overlay", stalled.frameid, stalled.cmd);
				}
			})
			this.overlayWindow = { browser, nativewnd, pin, stalledOverlay: [{ frameid: frameid, cmd: commands }] };
		} else {
			this.overlayWindow.browser.webContents.send("overlay", frameid, commands);
		}
	}
}