import { ImgRef } from "@alt1/base";
import * as OCR from "@alt1/ocr";

let chatfont = require("@alt1/ocr/dist/fonts/chat_8px.js");
let rightclickfont = require("../rightclick/imgs/rightclick.fontmeta.json");

type TextResult = {
	type: "text",
	font: "11pt" | "13pt" | "15pt" | "17pt" | "rightclick",
	line: ReturnType<typeof OCR["findReadLine"]>
}

export function readAnything(img: ImageData, x: number, y: number) {
	let text11pt = OCR.findReadLine(img, chatfont, [[255, 255, 255]], x, y);
	if (text11pt.text) {
		return { type: "text", font: "11pt", line: text11pt } as TextResult;
	}
	let textrightclick = OCR.findReadLine(img, rightclickfont, [[198, 184, 149], [248, 213, 107], [184, 209, 209], [255, 255, 255]], x, y);
	if (textrightclick.text) {
		return { type: "text", font: "rightclick", line: textrightclick } as TextResult;
	}
	//TODO other fonts

	//TODO other alt1+1-able things


	return null
}