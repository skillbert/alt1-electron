//this import also imports global namespace alt1 passively...
import type * as alt1types from "@alt1/base";
import { ipcRenderer } from "electron";
import { FlatImageData, SyncResponse, Rectangle, OverlayCommand, OverlayPrimitive } from "../shared";

let warningsTriggered: string[] = [];
function warn(key: string, message: string) {
	if (!warningsTriggered.includes(key)) {
		console.warn(message);
		warningsTriggered.push(key);
	}
}

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
	//3 bytes -> 4 chars repeating pattern
	for (var a = 0; a + 3 < data.length; a += 3) {
		let b1 = data[a + 0], b2 = data[a + 1], b3 = data[a + 2];
		str += btoachars[(b1 >> 2) & 0x3f] + btoachars[((b1 << 4) | (b2 >> 4)) & 0x3f] + btoachars[((b2 << 2) | (b3 >> 6)) & 0x3f] + btoachars[(b3) & 0x3f];
	}
	//use naive approach for left over bytes and let native btoa deal with ending bytes
	let endstr = "";
	for (; a < img.data.length; a++) {
		endstr += String.fromCharCode(data[a]);
	}
	str += btoa(endstr);
	return str;
}

let lastRsInfo: SyncResponse<Rectangle> = null!;
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

//TODO remove
(window as any).invoke = (ch: string, ...args: any[]) => {
	return ipcRenderer.invoke(ch, ...args);
}

//TODO use contextBridge.exposeInMainWorld
var alt1api: Partial<typeof alt1> = {

	identifyAppUrl: (url) => ipcRenderer.send("identifyapp", url),
	captureInterval: 100,
	maxtransfer: 10000000,
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
	overLayTextEx(text, color, size, x, y, time, font, center, shadow) {
		queueOverlayCommand({ command: "draw", time, action: { type: "text", x, y, font, text, center, shadow, color, size } });
		return true;
	},
	overLayText(text, color, size, x, y, time) {
		return alt1.overLayTextEx(text, color, size, x, y, time, "", false, true);
	},
	overLayImage(x, y, imgstr, imgwidth, time) {
		throw new Error("not implemented");
	},
	overLaySetGroup(groupid: string) { queueOverlayCommand({ command: "setgroup", groupid }); },
	overLaySetGroupZIndex(groupid: string, zindex: number) { queueOverlayCommand({ command: "setgroupzindex", groupid, zindex }); },
	overLayFreezeGroup(groupid) { queueOverlayCommand({ command: "freezegroup", groupid }); },
	overLayContinueGroup(groupid) { queueOverlayCommand({ command: "continuegroup", groupid }); },
	overLayClearGroup(groupid) { queueOverlayCommand({ command: "cleargroup", groupid }); },
	overLayRefreshGroup(groupid) { queueOverlayCommand({ command: "refreshgroup", groupid }); },
	setTooltip(str) { return true; },
	clearTooltip() { alt1api.setTooltip!(""); }
};

//API extension for fast capture
//declared here to not break outdated window.alt1 typing
let extendedapi = {
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
	}
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

Object.defineProperties(alt1api, {
	rsX: { get() { return getRsInfo()?.x || 0; } },
	rsY: { get() { return getRsInfo()?.y || 0; } },
	rsWidth: { get() { return getRsInfo()?.width || 0; } },
	rsHeight: { get() { return getRsInfo()?.height || 0; } },
	rsLinked: { get() { return true; } },
	currentWorld: { get() { return 1; } }
});

Object.assign(alt1api, extendedapi);

(window as any).alt1 = alt1api;