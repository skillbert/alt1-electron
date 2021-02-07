import * as path from "path";
import * as fs from "fs";

export type RsInfo = { x: number, y: number, width: number, height: number, title: string };
export type OSWindow = Uint8Array;


//Copy the addon file so we can rebuild while alt1lite is already running
let addonpath = path.resolve(__dirname, "../build/Debug/");
let tmpfile = path.resolve(addonpath, "addon" + Math.floor(Math.random() * 1000) + ".node");
let origfile = path.resolve(addonpath, "addon.node");
fs.copyFileSync(origfile, tmpfile);


var alt1native = __non_webpack_require__(tmpfile) as {
	captureDesktop: (x: number, y: number, w: number, h: number) => Uint8ClampedArray
	getWindowMeta: (hwnd: OSWindow) => { x: number, y: number, width: number, height: number }
	getProcessesByName: (name: string) => number[]
	getProcessMainWindow: (pid: number) => OSWindow,
	setPinParent: (window: OSWindow, newparent: OSWindow) => void
}

export const { getProcessMainWindow, captureDesktop, getProcessesByName, getWindowMeta, setPinParent } = alt1native;

