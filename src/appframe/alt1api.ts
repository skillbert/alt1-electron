//this import also imports global namespace alt1 passively...
import type * as alt1types from "@alt1/base";
import { ipcRenderer } from "electron";
import { FlatImageData, SyncResponse, OverlayCommand, RsClientState } from "../shared";
import {decodeImageString} from "@alt1/base";

let warningsTriggered: string[] = [];
function warn(key: string, message: string) {
	if (!warningsTriggered.includes(key)) {
		console.warn(message);
		warningsTriggered.push(key);
	}
}

ipcRenderer.on("appevent", <T extends keyof alt1types.Alt1EventType>(e, type: T, appevent: alt1types.Alt1EventType[T]) => {
	try {
		if (type == "alt1pressed" && (window as any).alt1onrightclick) {
			warn("alt1onrightclick", "window.alt1onrightclick is depricated use the wrapper lib instead");
			(window as any).alt1onrightclick(appevent);
		}
		if (alt1.events && Array.isArray(alt1.events[type])) {
			for (let handler of alt1.events[type]) {
				handler(appevent);
			}
		}
	} catch (e) {
		console.error(e);
	}
});

function captureSync(x: number, y: number, w: number, h: number) {
	warn("captsync", "Synchonous capture is depricated");
	let img: SyncResponse<FlatImageData> = ipcRenderer.sendSync("capturesync", x, y, w, h);
	if (img.error != undefined) { throw new Error(img.error); }
	return img.value;
}

function imagedataToBase64(img: FlatImageData) {
	warn("base64capt", "This capture api is a backward port for compatibylity and is much slower");
	const btoachars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	let str = "";
	let data = img.data;
	//flip rgba to bgra and convert into base64 in one pass
	//12 bytes = 3 pixels -> 16 chars repeating pattern
	//[bgra][bgra][bgra] -> [rgb][arg][bar][gba] (3 bytes encodes into 4 base64 chars)
	for (var a = 0; a + 3 < data.length; a += 12) {
		let b0 = data[a + 0], g0 = data[a + 1], r0 = data[a + 2], a0 = data[a + 3];
		let b1 = data[a + 4], g1 = data[a + 5], r1 = data[a + 6], a1 = data[a + 7];
		let b2 = data[a + 8], g2 = data[a + 9], r2 = data[a + 10], a2 = data[a + 11];
		str += btoachars[(r0 >> 2) & 0x3f] + btoachars[((r0 << 4) | (g0 >> 4)) & 0x3f] + btoachars[((g0 << 2) | (b0 >> 6)) & 0x3f] + btoachars[(b0) & 0x3f];
		str += btoachars[(a0 >> 2) & 0x3f] + btoachars[((a0 << 4) | (r1 >> 4)) & 0x3f] + btoachars[((r1 << 2) | (g1 >> 6)) & 0x3f] + btoachars[(g1) & 0x3f];
		str += btoachars[(b1 >> 2) & 0x3f] + btoachars[((b1 << 4) | (a1 >> 4)) & 0x3f] + btoachars[((a1 << 2) | (r2 >> 6)) & 0x3f] + btoachars[(r2) & 0x3f];
		str += btoachars[(g2 >> 2) & 0x3f] + btoachars[((g2 << 4) | (b2 >> 4)) & 0x3f] + btoachars[((b2 << 2) | (a2 >> 6)) & 0x3f] + btoachars[(a2) & 0x3f];
	}
	//use naive approach for left over bytes and let native btoa deal with ending bytes
	let endstr = "";
	for (; a < img.data.length; a++) {
		endstr += String.fromCharCode(data[a]);
	}
	str += btoa(endstr);
	return str;
}

let lastRsInfo: SyncResponse<RsClientState> = null!;
let lastRsInfoTime = 0;
function getRsInfo() {
	let info = lastRsInfo;
	if (lastRsInfoTime < Date.now() - 500) {
		info = ipcRenderer.sendSync("rsbounds");
		lastRsInfo = info;
		lastRsInfoTime = Date.now();
	}
	if (info.error != undefined) { throw new Error(info.error); }
	return info.value;
}

let boundImage: FlatImageData & { x: number, y: number } | null = null;
let overlayDebounceCommands: OverlayCommand[] = [];
let overlayDebounced = false;
function queueOverlayCommand(command: OverlayCommand) {
	overlayDebounceCommands.push(command);
	if (!overlayDebounced) {
		setImmediate(sendOverlayQueue);
		overlayDebounced = true;
	}
}
function sendOverlayQueue() {
	ipcRenderer.send("overlay", overlayDebounceCommands)
	overlayDebounced = false;
	overlayDebounceCommands = [];
}

function setTooltip(text: string) {
	ipcRenderer.send("settooltip", text);
}

//TODO use contextBridge.exposeInMainWorld
var alt1api: Partial<typeof alt1> = {

	identifyAppUrl: (url) => ipcRenderer.send("identifyapp", url),
	captureInterval: 100,
	maxtransfer: 100e6,
	openInfo: '{"openMethod":"systray"}',
	skinName: "default",
	version: "1.3.0",//old-ish version because of missing apis
	versionint: 1003000,
	openBrowser: (url) => { window.open(url, "_blank"); return true; },
	getRegion: (x, y, w, h) => {
		let img = captureSync(x, y, w, h);
		return imagedataToBase64(img);
	},
	bindRegion(x, y, w, h) {
		warn("bindbypass", "This platform does not utilise the bound image pattern, bound images are the same speed as normal capture");
		boundImage = { x, y, ...captureSync(x, y, w, h) };
		return 1;
	},
	bindGetPixel(id, x, y) {
		//TODO double check if this is implemented as error or not
		if (!boundImage || id != 1) { return 0; }
		let i = boundImage.width * 4 * y + 4 * x;
		let r = boundImage.data[i + 0], g = boundImage.data[i + 1], b = boundImage.data[i + 2], a = boundImage.data[i + 3];
		return (r << 24) | (g << 16) | (b << 8) | a;
	},
	bindGetRegion(id, x, y, w, h) {
		//TODO double check if this is implemented as error or not
		if (!boundImage || id != 1) { return ""; }
		return imagedataToBase64(subImageData(boundImage, x, y, w, h));
	},
	overLayLine(color, linewidth, x1, y1, x2, y2, time) {
		queueOverlayCommand({ command: "draw", time, action: { type: "line", x1, y1, x2, y2, color, linewidth } });
		return true;
	},
	overLayRect(color, x, y, width, height, time, linewidth) {
		queueOverlayCommand({ command: "draw", time, action: { type: "rect", x, y, width, height, color, linewidth } });
		return true;
	},
	overLayRectFill(color, fillColor, x, y, width, height, time, linewidth) {
		queueOverlayCommand({ command: "draw", time, action: { type: "rectfill", x, y, width, height, color, fillColor, linewidth } });
		return true;
	},
	overLayTextEx(text, color, size, x, y, time, font, center, shadow) {
		queueOverlayCommand({ command: "draw", time, action: { type: "text", x, y, font, text, center, shadow, color, size } });
		return true;
	},
	overLayText(text, color, size, x, y, time) {
		return alt1.overLayTextEx(text, color, size, x, y, time, "", false, true);
	},
	overLayImage(x, y, imgstr, imgwidth, time) {
		let imgheight = imgstr.length / (4 * imgwidth);
		let id = new ImageData(imgwidth, imgheight);
		let img = decodeImageString(imgstr, id, 0, 0, imgwidth, imgheight);
		queueOverlayCommand({ command: "draw", time, action: { type: "sprite", x, y, sprite: {data:img.data, width:imgwidth, height:img.height}} })
		return true
	},
	overLaySetGroup(groupid: string) { queueOverlayCommand({ command: "setgroup", groupid }); },
	overLaySetGroupZIndex(groupid: string, zindex: number) { queueOverlayCommand({ command: "setgroupzindex", groupid, zindex }); },
	overLayFreezeGroup(groupid) { queueOverlayCommand({ command: "freezegroup", groupid }); },
	overLayContinueGroup(groupid) { queueOverlayCommand({ command: "continuegroup", groupid }); },
	overLayClearGroup(groupid) { queueOverlayCommand({ command: "cleargroup", groupid }); },
	overLayRefreshGroup(groupid) { queueOverlayCommand({ command: "refreshgroup", groupid }); },
	setTooltip(str) { setTooltip(str); return true; },
	clearTooltip() { setTooltip(""); },

	//new API's
	capture(x, y, width, height) {
		return captureSync(x, y, width, height).data;
	},
	async captureAsync(x, y, width, height) {
		let r = await ipcRenderer.invoke("capture", x, y, width, height);
		return r;
	},
	async captureMultiAsync(areas) {
		let r = await ipcRenderer.invoke("capturemulti", areas);
		return r;
	},
	bindGetRegionBuffer(id, x, y, w, h) {
		if (!boundImage || id != 1) { throw new Error("no bound image"); }
		return subImageData(boundImage, x, y, w, h).data;
	},
	closeApp() {
		//TODO check if this actually works
		console.log("Close (alt1api.ts)");
		window.close();
	},
	userResize(left, top, right, bot) {
		ipcRenderer.sendSync("dragwindow", left, top, right, bot);
	}

	//TODO
	// bindFindSubImg: ,
	// getRegionMulti: ,
	// registerStatusDaemon: ,
	// showNotification: ,
	// setTaskbarProgress: ,
	// setTitleBarText: ,

	//no plans to implement
	// addOCRFont: ,
	// bindReadColorString: ,
	// bindReadRightClickString: ,
	// bindReadString: ,
	// bindReadStringEx: ,
	// bindScreenRegion: ,
	// clearBinds: ,

};

function subImageData(img: FlatImageData, x: number, y: number, w: number, h: number) {
	if (x == 0 && y == 0 && w == img.width && h == img.height) {
		return img;
	}
	let newdata = new Uint8ClampedArray(w * h * 4);
	let data = img.data;
	let imgwidth = img.width;
	for (let dy = 0; dy < h; dy++) {
		let i = x * 4 + (y + dy) * 4 * imgwidth;
		newdata.set(data.subarray(i, i + w * 4), dy * 4 * w);
	}
	return { data: newdata, width: w, height: h } as FlatImageData;
}

let getters: PropertyDescriptorMap = {
	rsX: { get() { return getRsInfo().clientRect.x; } },
	rsY: { get() { return getRsInfo().clientRect.y; } },
	rsWidth: { get() { return getRsInfo().clientRect.width; } },
	rsHeight: { get() { return getRsInfo().clientRect.height; } },
	rsActive: { get() { return getRsInfo().active; } },
	rsLastActive: { get() { return Date.now() - getRsInfo().lastActiveTime; } },
	rsPing: { get() { return getRsInfo().ping; } },
	rsScaling: { get() { return getRsInfo().scaling; } },
	rsLinked: { get() { return true; } }, //can no longer open apps without rs
	captureMethod: { get() { return getRsInfo().captureMode; } },
	//TODO
	currentWorld: { get() { return 1; } },
	lastWorldHop: { get() { return 0; } },
	permissionGameState: { get() { return true; } },
	permissionInstalled: { get() { return true; } },
	permissionOverlay: { get() { return true; } },
	permissionPixel: { get() { return true; } }
};

Object.defineProperties(alt1api, getters);

//TODO need some changes to make this work
//contextBridge.exposeInMainWorld("alt1", alt1api);
(window as any).alt1 = alt1api;
