import { ipcRenderer } from "electron";
import type { Settings, Bookmark } from "../settings";
import type { CaptureMode } from "../native";
import * as React from "react";
import * as ReactDom from "react-dom";
import "../appframe/alt1api";
import * as a1lib from "alt1";
import { runCaptureDiagnostic } from "../readers/capturediagnostic";
import "./style.scss";
import "./index.html";

window.addEventListener("DOMContentLoaded", start);

async function start() {
	ReactDom.render(<SettingsComponent />, document.getElementById("root"));
}

function SettingsComponent() {
	let [tab, setTab] = React.useState<"apps" | "capture">("apps");
	let [settings, updateSettings] = React.useState<Settings | null>(null);

	React.useEffect(() => {
		let onchange = async () => updateSettings(await ipcRenderer.invoke("getsettings"));
		ipcRenderer.on("settings-changed", onchange);
		onchange();
		return () => { ipcRenderer.off("settings-changed", onchange); }
	}, [updateSettings]);

	let content = <h1>Invalid tab</h1>;

	if (tab == "apps" && settings) {
		content = <AppSettings bookmarks={settings.bookmarks} />
	}
	if (tab == "capture" && settings) {
		content = <CaptureSettings settings={settings} />
	}

	return (
		<React.Fragment>
			<button onClick={() => setTab("apps")}>Apps</button>
			<button onClick={() => setTab("capture")}>Capture</button>
			<hr />
			{content}
		</React.Fragment>
	);
}

interface AppSettingsProps {
	bookmarks: Array<Bookmark>;
}

interface AppSettingsState {
	configUrl: string;
}

class AppSettings extends React.Component<AppSettingsProps, AppSettingsState> {
	constructor(props) {
		super(props);
		this.state = {
			configUrl: ""
		}
	}

	addAppSubmit(e) {
		ipcRenderer.invoke("installapp", this.state.configUrl);
		this.setState({ configUrl: "" });
		e.preventDefault();
	}

	render() {
		let apps = this.props.bookmarks.map(i => {
			return <tr key={i.appUrl}>
				<td><img width={20} height={20} src={i.iconCached} /></td>
				<td><span>{i.appName}</span></td>
				<td><button onClick={() => ipcRenderer.invoke("openapp", i.configUrl)}>Open</button></td>
				<td><button onClick={() => ipcRenderer.invoke("removeapp", i.configUrl)}>Remove</button></td>
			</tr>;
		})

		return <React.Fragment>
			<table><tbody>{apps}</tbody></table>
			<hr />
			<form onSubmit={this.addAppSubmit.bind(this)}>
				<label>Config URL <input type="text" value={this.state.configUrl} onChange={e => this.setState({ configUrl: e.target.value })} /></label>
				<button type="submit">Add App</button>
			</form>
		</React.Fragment>;
	}
}


function CaptureSettings(props: { settings: Settings }) {
	let modechange = (e: React.ChangeEvent<HTMLInputElement>) => {
		let captureMode = e.currentTarget.value as CaptureMode;
		ipcRenderer.invoke("setcapturemode", captureMode);
	}

	return (
		<React.Fragment>
			<p>Capture mode</p>
			<label><input type="radio" value="opengl" name="captmode" onChange={modechange} checked={props.settings.captureMode == "opengl"} />OpenGL</label>
			<label><input type="radio" value="window" name="captmode" onChange={modechange} checked={props.settings.captureMode == "window"} />Window</label>
			<label><input type="radio" value="desktop" name="captmode" onChange={modechange} checked={props.settings.captureMode == "desktop"} />Desktop</label>
			<CapturePreview mode={props.settings.captureMode} />
		</React.Fragment>
	);
}

function CapturePreview(p: { mode: CaptureMode }) {
	let [diag, setdiagnostic] = React.useState<ReturnType<typeof runCaptureDiagnostic> | null>(null);

	let reffnc = React.useMemo(() => {
		let interval = 0;
		return (cnv: HTMLCanvasElement | null) => {
			if (interval) { clearInterval(interval); }
			if (cnv) {
				let ctx = cnv.getContext("2d")!;
				let render = async () => {
					let img = await a1lib.captureAsync(0, 0, alt1.rsWidth, alt1.rsHeight);
					cnv.width = img.width;
					cnv.height = img.height;
					ctx.putImageData(img, 0, 0);
					setdiagnostic(runCaptureDiagnostic(new a1lib.ImgRefData(img)));
				}
				interval = +setInterval(render, 100);
				render();
			}
		}
	}, [p.mode]);

	return (
		<React.Fragment>
			<canvas ref={reffnc} style={{ maxWidth: "500px", maxHeight: "500px" }} />
			{diag && <div>{
				diag.homeportfound ? "Home teleport button found, capture seems to be working correctly!" : "Alt1 failed to find the home teleport button."
			}</div>}
		</React.Fragment>
	)
}