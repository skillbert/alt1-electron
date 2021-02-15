//figure this one out for jsx
import * as React from "react";
import { useState, useLayoutEffect, useRef } from "react";
import { render } from "react-dom";
import { ipcRenderer, remote, WebContents, WebviewTag } from "electron";
import classnames from "classnames";

import "./style.scss";
import "./index.html";

(window as any).remote = remote;
var appview: WebviewTag | null = null;
var appcontents: WebContents | null = null;
var mainmodule = remote.getGlobal("Alt1lite") as typeof import("../main");
//TODO backup if this fails
var thiswindow = mainmodule.getManagedWindow(remote.getCurrentWebContents())!;

window.addEventListener("DOMContentLoaded", () => {
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
		view.src = thiswindow.appConfig.appUrl;
		view.webpreferences = "nativeWindowOpen,sandbox";
		el.current.appendChild(view);
		view.addEventListener("dom-ready", e => {
			//TODO is there a better way to get a ref to the frame?
			thiswindow.appFrameId = view.getWebContentsId();
		});

		appview = view;
		view.addEventListener("dom-ready", () => {
			appcontents = remote.webContents.fromId(appview!.getWebContentsId());
		});
		//setparent doesnt work as expected
		// view.addEventListener("devtools-opened", e => {
		// 	let devwnd = (appcontents!.devToolsWebContents as any).getOwnerBrowserWindow();
		// 	let selfwnd = remote.getCurrentWindow();
		// 	if (devwnd && selfwnd) { devwnd.setParentWindow(selfwnd); }
		// });
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
		let initial = thiswindow.nativeWindow.getBounds();
		starte.preventDefault();
		starte.stopPropagation();
		appview!.style.pointerEvents = "none";
		let startpos = remote.screen.getCursorScreenPoint();
		let moved = () => {
			let pos = remote.screen.getCursorScreenPoint();
			let dx = pos.x - startpos.x;
			let dy = pos.y - startpos.y;
			thiswindow.nativeWindow.setBounds(initial.x + dx * factors.x, initial.y + dy * factors.y, initial.width + dx * factors.w, initial.height + dy * factors.h);
			thiswindow.windowPin.updateDocking();
		};
		let cleanup = () => {
			window.removeEventListener("mousemove", moved);
			appview!.style.pointerEvents = "";
		}
		window.addEventListener("mousemove", moved);
		window.addEventListener("mouseup", cleanup, { once: true });
	}
}