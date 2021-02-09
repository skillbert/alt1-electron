import * as path from "path";
import * as fs from "fs";
import { Rectangle } from "./shared";

export type OSWindow = {
	getTitle(): string
	setBounds(x: number, y: number, w: number, h: number): void
	getBounds(): Rectangle
	getClientBounds(): Rectangle
	setPinParent(parent: OSWindow): OSWindowPin
	equals(other: OSWindow): boolean
};
export type OSWindowPin = {
	unpin(): void
	updatePinAnchor(): void
}


//Copy the addon file so we can rebuild while alt1lite is already running
let addonpath = path.resolve(__dirname, "../build/Debug/");
let tmpfile = path.resolve(addonpath, "addon" + Math.floor(Math.random() * 1000) + ".node");
let origfile = path.resolve(addonpath, "addon.node");
fs.copyFileSync(origfile, tmpfile);


var alt1native = __non_webpack_require__(tmpfile) as {
	captureDesktop: (x: number, y: number, w: number, h: number) => Uint8ClampedArray,
	captureDesktopMulti: <T extends { [key: string]: Rectangle | undefined | null }>(rect: T) => { [key in keyof T]: Uint8ClampedArray },
	getProcessMainWindow: (pid: number) => OSWindow,
	getProcessesByName: (name: string) => number[],
	OSWindow: new (handle: Uint8Array) => OSWindow
}

export const { getProcessMainWindow, captureDesktopMulti, captureDesktop, getProcessesByName, OSWindow } = alt1native;

