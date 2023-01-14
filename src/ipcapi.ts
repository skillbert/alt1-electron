import * as a1lib from "@alt1/base/dist";
import { IpcMain, IpcMainEvent, IpcMainInvokeEvent, screen } from "electron/main"
import { identifyApp, uninstallApp } from "./appconfig";
import { sameDomainResolve } from "./lib";
import { admins, fixTooltip, getManagedAppWindow, ManagedWindow, openApp } from "./main";
import { native } from "./native";
import { settings } from "./settings";
import { FlatImageData, OverlayCommand, Rectangle, RsClientState } from "./shared";

const snapdistance = 10;
const snapcornerlength = 30;
const snapthresh = 140;

function expectAppWindow(e: IpcMainEvent | IpcMainInvokeEvent) {
	let wnd = getManagedAppWindow(e.sender.id);
	if (!wnd) { throw new Error("App context not found"); }
	//TODO check if e.senderFrame.url is same origin as appconfig
	return wnd;
}

function isAdmin(e: IpcMainEvent | IpcMainInvokeEvent) {
	return admins.indexOf(e.sender.id) != -1;
}

function detectCornerEdge(img: FlatImageData, rect: a1lib.Rect, hor: boolean, reverse: boolean, thresh: number) {
	if (!hor) {
		var rect1 = new a1lib.Rect(rect.x, rect.y, rect.width, snapcornerlength);
		var rect2 = new a1lib.Rect(rect.x, rect.y + rect.height - snapcornerlength, rect.width, snapcornerlength);
	} else {
		var rect1 = new a1lib.Rect(rect.x, rect.y, snapcornerlength, rect.height);
		var rect2 = new a1lib.Rect(rect.x + rect.width - snapcornerlength, rect.y, snapcornerlength, rect.height);
	}
	let edgetop = detectEdge(img, rect1, hor, reverse, thresh);
	let edgebot = detectEdge(img, rect2, hor, reverse, thresh);
	return (edgetop.score > edgebot.score ? edgetop : edgebot);
}

function detectEdge(img: FlatImageData, rect: a1lib.Rect, hor: boolean, reverse: boolean, thresh: number) {
	let originalsize = (hor ? rect.width : rect.height);
	rect.intersect(new a1lib.Rect(0, 0, img.width, img.height));

	let scanstride = (hor ? img.width * 4 : 4);
	let scancount = (hor ? rect.height : rect.width) - 1;
	let secondstride = (hor ? 4 : img.width * 4);
	let secondcount = (hor ? rect.width : rect.height);

	let startindex = rect.x * 4 + rect.y * img.width * 4;

	let posbase = (hor ? rect.y : rect.x);

	let best = {
		score: 0,
		pos: posbase + (reverse ? hor ? rect.height : rect.width : 0)
	};
	for (let scanstep = 0; scanstep <= scancount; scanstep++) {
		let dsum = 0;
		let scanindex = (reverse ? scancount - scanstep : scanstep);
		let ibase = startindex + scanindex * scanstride;
		for (let step = 0; step < secondcount; step++) {
			let i1 = ibase + step * secondstride;
			let i2 = ibase + scanstride + step * secondstride;

			dsum += Math.abs(img.data[i1 + 0] - img.data[i2 + 0]);
			dsum += Math.abs(img.data[i1 + 1] - img.data[i2 + 1]);
			dsum += Math.abs(img.data[i1 + 2] - img.data[i2 + 2]);
		}
		let score = dsum / originalsize;
		if (score > best.score) {
			best.pos = posbase + scanindex + 1;
			best.score = score;
		}
		if (score > thresh) {
			return best;
		}
	}

	//also treat the window bounds as edges
	if (!hor && !reverse && rect.x + rect.width == img.width) { best.pos = img.width; best.score = 1000; }
	if (hor && !reverse && rect.y + rect.height == img.height) { best.pos = img.height; best.score = 1000; }
	if (!hor && reverse && rect.x == 0 && rect.width != 0) { best.pos = 0; best.score = 1000; }
	if (hor && reverse && rect.y == 0 && rect.height != 0) { best.pos = 0; best.score = 1000; }

	return best;
}

function startDrag(wnd: ManagedWindow, left: boolean, top: boolean, right: boolean, bot: boolean) {
	top ??= false; left ??= false; right ??= false; bot ??= false;

	//TODO get these from global constants/appconfig
	const minwidth = 140;
	const minheight = 50;

	let rsbounds = wnd.rsClient.window.getClientBounds();
	let startpos = screen.getCursorScreenPoint();
	let initial = wnd.window.getBounds();
	initial.x -= rsbounds.x;
	initial.y -= rsbounds.y;
	let lastx = startpos.x;
	let lasty = startpos.y;
	let dirx = 0;
	let diry = 0;

	//TODO display scaling
	let imgdata = native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, { main: { x: 0, y: 0, width: rsbounds.width, height: rsbounds.height } }).main;
	let img: FlatImageData = { data: imgdata, width: rsbounds.width, height: rsbounds.height };

	let tick = () => {
		//can't rely on any window events for this since were crossing like 5 processes and 23 threads
		if (!native.getMouseState()) {
			clearInterval(interval);
			return;
		}
		let pos = screen.getCursorScreenPoint();
		let dx = pos.x - startpos.x;
		let dy = pos.y - startpos.y;

		let wndleft = initial.x + dx * +left;
		let wndtop = initial.y + dy * +top;
		let wndright = initial.x + initial.width + dx * +right;
		let wndbot = initial.y + initial.height + dy * +bot;

		//dragging direction with some slight hysteresis
		dirx = Math.sign(pos.x - lastx) || dirx;
		diry = Math.sign(pos.y - lasty) || diry;

		let snapdx = 0;
		let snapdy = 0;

		if (dirx > 0 && right) {
			let rect = new a1lib.Rect(wndright, wndtop, snapdistance, wndbot - wndtop);
			let edge = detectCornerEdge(img, rect, false, false, snapthresh);
			if (edge.score > snapthresh) { snapdx = edge.pos - wndright; }
		}
		if (dirx < 0 && left) {
			let rect = new a1lib.Rect(wndleft - snapdistance, wndtop, snapdistance, wndbot - wndtop);
			let edge = detectCornerEdge(img, rect, false, true, snapthresh);
			if (edge.score > snapthresh) { snapdx = edge.pos - wndleft; }
		}
		if (diry > 0 && bot) {
			let rect = new a1lib.Rect(wndleft, wndbot, wndright - wndleft, snapdistance);
			let edge = detectCornerEdge(img, rect, true, false, snapthresh);
			if (edge.score > snapthresh) { snapdy = edge.pos - wndbot; }
		}
		if (diry < 0 && top) {
			let rect = new a1lib.Rect(wndright, wndtop - snapdistance, wndright - wndleft, snapdistance);
			let edge = detectCornerEdge(img, rect, true, true, snapthresh);
			if (edge.score > snapthresh) { snapdy = edge.pos - wndtop; }
		}

		//apply snap displacement
		wndleft += snapdx * +left;
		wndtop += snapdy * +top;
		wndright += snapdx * +right;
		wndbot += snapdy * +bot;

		//ensure the new size isn't too small
		if (wndright - wndleft < minwidth) {
			wndleft -= (minwidth - (wndright - wndleft)) * +left
			wndright += (minwidth - (wndright - wndleft)) * +right;
		}
		if (wndbot - wndtop < minheight) {
			wndtop -= (minheight - (wndbot - wndtop)) * +top
			wndbot += (minheight - (wndbot - wndtop)) * +bot;
		}

		wnd.window.setBounds({
			x: wndleft + rsbounds.x,
			y: wndtop + rsbounds.y,
			width: wndright - wndleft,
			height: wndbot - wndtop,
		});

		wnd.windowPin.updateDocking();
		wnd.appConfig.lastRect = wnd.windowPin.getPinRect();
		lastx = pos.x;
		lasty = pos.y;
	};

	let interval = setInterval(tick, 20);
}

function syncwrap(fn: (e: Electron.IpcMainEvent, ...args: any[]) => any) {
	return (e: Electron.IpcMainEvent, ...args: any[]) => {
		try {
			let res = fn(e, ...args);
			e.returnValue = { value: res };
		} catch (err) {
			e.returnValue = { error: err + "" };
		}
	}
}

export function initIpcApi(ipcMain: IpcMain) {
	ipcMain.on("identifyapp", async (e, configurl) => {
		try {
			let url = sameDomainResolve(e.sender.getURL(), configurl);
			await identifyApp(url);
		} catch (e) {
			console.error(e);
		}
	});

	ipcMain.on("capturesync", (e, x, y, width, height) => {
		try {
			let wnd = expectAppWindow(e);
			let capt = native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, { main: { x, y, width, height } });
			e.returnValue = { value: { width, height, data: capt.main } };
		} catch (err) {
			e.returnValue = { error: "" + err };
		}
	});

	ipcMain.on("rsbounds", syncwrap((e) => {
		let wnd = expectAppWindow(e);
		let state: RsClientState = {
			active: wnd.rsClient.isActive,
			clientRect: wnd.rsClient.window.getClientBounds(),
			lastActiveTime: wnd.rsClient.lastActiveTime,
			ping: 10,//TODO
			scaling: 1,//TODO
			captureMode: settings.captureMode
		};
		e.returnValue = { value: state };
	}));

	ipcMain.handle("capture", (e, x, y, width, height) => {
		let wnd = expectAppWindow(e);
		return native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, { main: { x, y, width, height } }).main;
	});

	ipcMain.handle("capturemulti", (e, rects: { [key: string]: Rectangle }) => {
		let wnd = expectAppWindow(e);
		return native.captureWindowMulti(wnd.rsClient.window.handle, settings.captureMode, rects);
	});

	ipcMain.on("settooltip", syncwrap((e, text: string) => {
		let wnd = expectAppWindow(e);
		wnd.activeTooltip = text;
		fixTooltip();
	}));

	ipcMain.on("overlay", syncwrap((e, commands: OverlayCommand[]) => {
		//TODO errors here are not rethrown in app, just swallow and log them
		let wnd = expectAppWindow(e);
		wnd.rsClient.overlayCommands(wnd.appFrameId, commands);
	}));

	ipcMain.on("dragwindow", syncwrap((e, left, top, right, bot) => {
		let wnd = expectAppWindow(e);
		startDrag(wnd, left, top, right, bot);
	}));

	ipcMain.on("shape", syncwrap((e, wnd: BigInt, rects: Rectangle[]) => {
		native.setWindowShape(wnd, rects);
	}));

	ipcMain.handle("openapp", async (e, url) => {
		if (isAdmin(e)) {
			let app = settings.bookmarks.find(a => a.configUrl == url);
			if (!app) {
				return { error: "App not found" };
			} else {
				openApp(app);
				return {};
			}
		}
	});

	ipcMain.handle("removeapp", async (e, url) => {
		if (isAdmin(e)) {
			uninstallApp(url);
		}
	});

	ipcMain.handle("installapp", async (e, url) => {
		if (isAdmin(e)) {
			await identifyApp(new URL(url));
		}
	});

	ipcMain.handle("getsettings", (e) => {
		if (isAdmin(e)) {
			return settings;
		}
	})
}