//figure this one out for jsx
import * as React from "react";
import { useState, useLayoutEffect, useRef } from "react";
import { render } from "react-dom";
import { ipcRenderer, remote, WebContents, WebviewTag } from "electron";
import classnames from "classnames";

import "./style.scss";
import "./index.html";
import { InstalledApp } from "../appconfig";

var wnd = remote.getCurrentWindow();
var appview: WebviewTag | null = null;
var appcontents: WebContents | null = null;
var pageconfig: InstalledApp = null!;

Promise.all([
	ipcRenderer.invoke("whoami"),
	new Promise(done => window.addEventListener("DOMContentLoaded", done))
]).then(([appconfig]) => {
	pageconfig = appconfig;
	render(<AppFrame />, document.getElementById("root"));
});

function AppFrame(p: {}) {
	let el = useRef(null! as HTMLDivElement);

	useLayoutEffect(() => {
		let view = document.createElement("webview");
		view.className = "appframe";
		view.preload = "./alt1api.bundle.js";
		view.enableremotemodule = false;
		view.allowpopups = true;
		view.nodeintegration = false;
		view.nodeintegrationinsubframes = false;
		view.src = pageconfig.appUrl;
		view.webpreferences = "nativeWindowOpen,sandbox";
		el.current.appendChild(view);

		appview = view;
		view.addEventListener("dom-ready", () => {
			let id = appview!.getWebContentsId();
			appcontents = remote.webContents.getAllWebContents().find(q => q.id == id) || null;
		});
		(window as any).view = view;
		return () => { appview = null };
	}, []);

	return (
		<React.Fragment>
			<div className="approot" ref={el} >
				<BorderEl ver="top" hor="left" />
				<BorderEl ver="top" hor="" />
				<BorderEl ver="top" hor="right" />
				<BorderEl ver="" hor="left" />
				<BorderEl ver="" hor="right" />
				<BorderEl ver="bot" hor="left" />
				<BorderEl ver="bot" hor="" />
				<BorderEl ver="bot" hor="right" />
			</div>
			<div className="buttonroot">
				<div className="button" onClick={e => close()} />
				<div className="button" />
				<div className="button" onClick={toggleDevTools} onContextMenu={e => e.preventDefault()} />
				<div className="dragbutton" onMouseDown={startDrag({ x: 1, y: 1, w: 0, h: 0 })} />
			</div>
		</React.Fragment>
	);
}

function toggleDevTools(e: React.MouseEvent) {
	if (e.button == 0) {
		if (appcontents) {
			if (appcontents.isDevToolsOpened()) { appcontents.closeDevTools(); }
			else { appcontents.openDevTools({ mode: "detach" }); }
		}
	} else if (e.button == 2) {
		let cnt = remote.getCurrentWebContents()
		if (cnt.isDevToolsOpened()) { cnt.closeDevTools(); }
		else { cnt.openDevTools({ mode: "detach" }); }
	}
}

function BorderEl(p: { ver: "top" | "bot" | "", hor: "left" | "right" | "" }) {
	return <div className={classnames("border", "border-" + p.ver + p.hor)} onMouseDown={borderDrag(p.ver, p.hor)}></div>
}

function borderDrag(ver: "top" | "bot" | "", hor: "left" | "right" | "") {
	let factors = {
		x: (hor == "left" ? 1 : 0),
		y: (ver == "top" ? 1 : 0),
		w: (hor == "right" ? 1 : hor == "left" ? -1 : 0),
		h: (ver == "bot" ? 1 : ver == "top" ? -1 : 0)
	};
	return startDrag(factors);
}

function startDrag(factors: { x: number, y: number, w: number, h: number }) {
	return function startDrag(starte: React.MouseEvent) {
		let initialsize = wnd.getSize();
		let initialpos = wnd.getPosition();
		starte.preventDefault();
		starte.stopPropagation();
		appview!.style.pointerEvents = "none";
		let startpos = remote.screen.getCursorScreenPoint();
		let moved = () => {
			let pos = remote.screen.getCursorScreenPoint();
			let dx = pos.x - startpos.x;
			let dy = pos.y - startpos.y;
			if (factors.x || factors.y) {
				wnd.setPosition(initialpos[0] + dx * factors.x, initialpos[1] + dy * factors.y);
			}
			if (factors.w || factors.h) {
				wnd.setSize(initialsize[0] + dx * factors.w, initialsize[1] + dy * factors.h);
			}
		};
		let cleanup = () => {
			window.removeEventListener("mousemove", moved);
			appview!.style.pointerEvents = "";
		}
		window.addEventListener("mousemove", moved);
		window.addEventListener("mouseup", cleanup, { once: true });
	}
}