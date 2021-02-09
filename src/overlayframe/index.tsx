import { ipcRenderer } from "electron/renderer";
import type { OverlayCommand, OverlayPrimitive } from "src/shared";

import "./index.html";

type ActivePrimitive = { endtime: number, visible: boolean, deleted: boolean, action: OverlayPrimitive };
type OverlayGroup = { name: string, frameid: number, zindex: number, frozen: boolean, primitives: ActivePrimitive[] };
type FrameState = { currentgroup: OverlayGroup; };

let groupstates: OverlayGroup[] = [];
let framestates = new Map<number, FrameState>();
let iszsorted = true;
let cnv = document.getElementById("cnv") as HTMLCanvasElement;
let ctx = cnv.getContext("2d")!;

function findFrameState(frameid: number) {
	let s = framestates.get(frameid);
	if (s) { return s; }
	s = { currentgroup: findgroup(frameid, "") };
	framestates.set(frameid, s);
	return s;
}

function findgroup(frameid: number, groupid: string) {
	let g = groupstates.find(q => q.name == groupid && q.frameid == frameid);
	if (g) { return g; }
	g = { name: groupid, frameid: frameid, frozen: false, zindex: 0, primitives: [] };
	groupstates.push(g);
	return g;
}

ipcRenderer.on("overlay", (e, frameid: number, commands) => {
	parseCommands(frameid, commands);
});

ipcRenderer.on("closeframe", (e, frameid: number) => {
	framestates.delete(frameid);
	groupstates = groupstates.filter(q => q.frameid == frameid);
	redraw(Date.now());
});

function parseCommands(frameid: number, commands: OverlayCommand[]) {
	let now = Date.now();
	let changed = false;
	let framestate = findFrameState(frameid);
	let currentgroup = framestate.currentgroup;
	for (let c of commands) {
		if (c.command == "draw") {
			currentgroup.primitives.push({ visible: !currentgroup.frozen, deleted: false, endtime: now + c.time, action: c.action });
			changed = true;
		} else if (c.command == "setgroup") {
			currentgroup = findgroup(frameid, c.groupid);
		} else if (c.command == "cleargroup") {
			let group = findgroup(frameid, c.groupid);
			if (group) { group.primitives.forEach(p => p.deleted = true); }
			if (!group.frozen) { changed = true; }
		} else if (c.command == "setgroupzindex") {
			let group = findgroup(frameid, c.groupid);
			group.zindex = c.zindex;
			iszsorted = false;
			changed = true;
		} else if (c.command == "freezegroup") {
			findgroup(frameid, c.groupid).frozen = true;
		} else if (c.command == "continuegroup" || c.command == "refreshgroup") {
			let group = findgroup(frameid, c.groupid);
			let oldfreeze = group.frozen;
			group.frozen = false;
			cleanGroup(group, now);
			if (c.command == "refreshgroup") {
				group.frozen = oldfreeze;
			}
			changed = true;
		}
	}

	if (changed) {
		redraw(now);
	}
}

function cleanGroup(g: OverlayGroup, now: number) {
	let endtime = (!g.frozen ? now : now + 10 * 1000);
	g.primitives = g.primitives.filter(p => !p.deleted && p.endtime <= endtime);
	//elements created during freeze
	for (let prim of g.primitives) { prim.visible = true; }
}

function coltocss(c: number) {
	return `rgb(${(c >> 24) & 0xff},${(c >> 16) & 0xff},${(c >> 8) & 0xff})`;
}

function redraw(now: number) {
	cnv.width = cnv.clientWidth;
	cnv.height = cnv.clientHeight;

	if (!iszsorted) {
		groupstates = groupstates.sort((a, b) => a.zindex - b.zindex);
		iszsorted = true;
	}

	for (let g of groupstates) {
		cleanGroup(g, now);
		for (let prim of g.primitives) {
			if (!prim.visible) { continue; }
			let act = prim.action;
			if (act.type == "line") {
				ctx.strokeStyle = coltocss(act.color);
				ctx.lineWidth = act.linewidth;
				ctx.moveTo(act.x1, act.y1);
				ctx.lineTo(act.x2, act.x2);
				ctx.stroke();
			} else if (act.type == "rect") {
				ctx.strokeStyle = coltocss(act.color);
				ctx.lineWidth = act.linewidth;
				ctx.strokeRect(act.x, act.y, act.width, act.height);
			} else if (act.type == "text") {
				ctx.fillStyle = coltocss(act.color);
				ctx.font = `${act.size}px ${act.font}`;
				ctx.textAlign = act.center ? "center" : "start";
				ctx.textBaseline = act.center ? "middle" : "top";
				ctx.fillText(act.text, act.x, act.y);
			} else if (act.type == "sprite") {
				//TODO
			}
		}
	}

	groupstates = groupstates.filter(q => q.primitives.length != 0 || !q.frozen || q.zindex != 0);
}
