import { ipcRenderer } from "electron/renderer";
import type { OverlayCommand, OverlayPrimitive } from "src/shared";

import "./index.html";

type ActivePrimitive = { endtime: number, visible: boolean, deleted: boolean, action: OverlayPrimitive };
type OverlayGroup = { name: string, frameid: number, zindex: number, frozen: boolean, primitives: ActivePrimitive[], nextupdate: number };
type FrameState = { currentgroup: OverlayGroup; };

let groupstates: OverlayGroup[] = [];
let framestates = new Map<number, FrameState>();
let iszsorted = true;
let cnv = document.getElementById("cnv") as HTMLCanvasElement;
let ctx = cnv.getContext("2d")!;
let redrawtimer = 0;
let shutdowntimer = 0;
const shutdowntimeout = 30 * 1000;
//js uses center of pixel definition

window.addEventListener("resize", e => redraw(Date.now(), true));

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
	g = { name: groupid, frameid: frameid, frozen: false, zindex: 0, primitives: [], nextupdate: Infinity };
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
	let framestate = findFrameState(frameid);
	let currentgroup = framestate.currentgroup;
	for (let c of commands) {
		if (c.command == "draw") {
			currentgroup.primitives.push({ visible: !currentgroup.frozen, deleted: false, endtime: now + c.time, action: c.action });
			if (!currentgroup.frozen) { currentgroup.nextupdate = 0; }
		} else if (c.command == "setgroup") {
			currentgroup = findgroup(frameid, c.groupid);
		} else if (c.command == "cleargroup") {
			let group = findgroup(frameid, c.groupid);
			group.primitives.forEach(p => p.deleted = true);
			if (!group.frozen) { group.nextupdate = 0; }
		} else if (c.command == "setgroupzindex") {
			let group = findgroup(frameid, c.groupid);
			group.zindex = c.zindex;
			iszsorted = false;
			group.nextupdate = 0;
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
			group.nextupdate = 0;
		}
	}

	redraw(now);
}

function cleanGroup(g: OverlayGroup, now: number) {
	const bonustime = (!g.frozen ? 0 : 10 * 1000);
	let endtime = now - bonustime;
	g.primitives = g.primitives.filter(p => (!p.deleted || g.frozen) && p.endtime > endtime);
	//elements created during freeze
	let nextupdate = Infinity;
	for (let prim of g.primitives) {
		if (!g.frozen) { prim.visible = true; }
		nextupdate = Math.min(nextupdate, prim.endtime + bonustime);
	}
	g.nextupdate = nextupdate;
}

function coltocss(c: number) {
	//ARGB
	return `rgb(${(c >> 16) & 0xff},${(c >> 8) & 0xff},${(c >> 0) & 0xff})`;
}

function scheduleRedraw(time: number) {
	if (redrawtimer) {
		clearTimeout(redrawtimer);
		redrawtimer = 0;
	}
	if (isFinite(time)) {
		redrawtimer = setTimeout(redraw, time - Date.now(), time) as any;
	}
}

function redraw(now: number, force = false) {
	if (!iszsorted) {
		groupstates = groupstates.sort((a, b) => a.zindex - b.zindex);
		iszsorted = true;
	}

	let currentnextupdate = Infinity;
	let newnextupdate = Infinity;
	for (let g of groupstates) {
		currentnextupdate = Math.min(currentnextupdate, g.nextupdate);
		cleanGroup(g, now);
		newnextupdate = Math.min(newnextupdate, g.nextupdate);
	}

	//remove obsolete groups
	groupstates = groupstates.filter(q => q.primitives.length != 0 || q.frozen || q.zindex != 0);

	let drawcount = 0;
	if (force || currentnextupdate <= now) {
		cnv.width = cnv.clientWidth;
		cnv.height = cnv.clientHeight;
		ctx.translate(0.5, 0.5);

		for (let g of groupstates) {
			for (let prim of g.primitives) {
				drawcount++;
				if (!prim.visible) { continue; }
				let act = prim.action;
				if (act.type == "line") {
					ctx.strokeStyle = coltocss(act.color);
					ctx.lineWidth = act.linewidth;
					ctx.moveTo(act.x1, act.y1);
					ctx.lineTo(act.x2, act.y2);
					ctx.stroke();
				} else if (act.type == "rect") {
					ctx.strokeStyle = coltocss(act.color);
					ctx.lineWidth = act.linewidth;
					ctx.strokeRect(act.x + act.linewidth / 2, act.y + act.linewidth / 2, act.width - act.linewidth, act.height - act.linewidth);
				} else if (act.type == "text") {
					ctx.fillStyle = coltocss(act.color);
					ctx.font = `${act.size}pt ${act.font || "sans-serif"}`;
					ctx.textAlign = act.center ? "center" : "start";
					ctx.textBaseline = act.center ? "middle" : "top";
					ctx.fillText(act.text, act.x, act.y);
				} else if (act.type == "sprite") {
					//TODO
				}
			}
		}
	}

	if (drawcount == 0 && !shutdowntimer) {
		shutdowntimer = setTimeout(e => window.close(), shutdowntimeout) as any;
	}
	if (drawcount != 0 && shutdowntimer) {
		clearTimeout(shutdowntimer);
		shutdowntimer = 0;
	}

	scheduleRedraw(newnextupdate);
}
