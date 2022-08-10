import { CaptureMode } from "./native";

export type FlatImageData = { data: Uint8ClampedArray, width: number, height: number };
export type SyncResponse<T> = { error: string } | { error: undefined, value: T };
export type Rectangle = { x: number, y: number, width: number, height: number };
export type RsClientState = {
	clientRect: Rectangle,
	active: boolean,
	lastActiveTime: number,
	ping: number,
	scaling: number,
	captureMode: CaptureMode
}


type DrawBase = {};

type DrawLine = DrawBase & { type: "line", color: number, linewidth: number, x1: number, y1: number, x2: number, y2: number };
type DrawRect = DrawBase & { type: "rect", color: number, linewidth: number, x: number, y: number, width: number, height: number };
type DrawText = DrawBase & { type: "text", color: number, size: number, text: string, font: string, shadow: boolean, center: boolean, x: number, y: number };
type DrawSprite = DrawBase & { type: "sprite", x: number, y: number, sprite: FlatImageData };

export type OverlayPrimitive = DrawLine | DrawRect | DrawText | DrawSprite;

type CommandBase = {}
type CommandDraw = CommandBase & { command: "draw", time: number, action: OverlayPrimitive };
type CommandSetGroup = CommandBase & { command: "setgroup", groupid: string };
type CommandClearGroup = CommandBase & { command: "cleargroup", groupid: string };
type CommandFreezeGroup = CommandBase & { command: "freezegroup", groupid: string };
type CommandContinueGroup = CommandBase & { command: "continuegroup", groupid: string };
type CommandRefreshGroup = CommandBase & { command: "refreshgroup", groupid: string };
type CommandSetgroupZindex = CommandBase & { command: "setgroupzindex", groupid: string, zindex: number };

export type OverlayCommand = CommandDraw | CommandSetGroup | CommandClearGroup | CommandFreezeGroup | CommandContinueGroup | CommandRefreshGroup | CommandSetgroupZindex;
