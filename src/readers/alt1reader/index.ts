import { ImgRefData, Rect } from "alt1";
import * as OCR from "alt1/ocr";
import RightClickReader from "../rightclick";

const chatfonts: { name: TextResult["font"], font: OCR.FontDefinition }[] = [
	{ name: "10pt", font: require("alt1/fonts/chatbox/10pt.js") },
	{ name: "12pt", font: require("alt1/fonts/chatbox/12pt.js") },
	{ name: "14pt", font: require("alt1/fonts/chatbox/14pt.js") },
	{ name: "16pt", font: require("alt1/fonts/chatbox/16pt.js") },
	{ name: "18pt", font: require("alt1/fonts/chatbox/18pt.js") },
];

type TextResult = {
	type: "text",
	font: "10pt" | "12pt" | "14pt" | "16pt" | "18pt",//larger than 18 has unreasonable perf cost
	line: ReturnType<typeof OCR["findReadLine"]>
}

type RightClickResult = {
	type: "rightclick",
	line: ReturnType<typeof OCR["findReadLine"]>,
	menu: ReturnType<InstanceType<typeof RightClickReader>["read"]>
}

//copied from alt1/chatbox
export const defaultcolors: OCR.ColortTriplet[] = [
	[0, 255, 0],
	[0, 255, 255],
	[0, 175, 255],
	[0, 0, 255],
	[255, 82, 86],
	[159, 255, 159],
	[0, 111, 0],
	[255, 143, 143],
	[255, 152, 31],
	[255, 111, 0],
	[255, 255, 0],
	//[239, 0, 0],//messes up broadcast detection [255,0,0]
	[239, 0, 175],
	[255, 79, 255],
	[175, 127, 255],
	//[48, 48, 48],//fuck this color, its unlegible for computers and people alike
	[191, 191, 191],
	[127, 255, 255],
	[128, 0, 0],
	[255, 255, 255],
	[127, 169, 255],
	[255, 140, 56], //orange drop received text
	[255, 0, 0], //red achievement world message
	[69, 178, 71], //blueish green friend broadcast
	[164, 153, 125], //brownish gray friends/fc/cc list name
	[215, 195, 119] //interface preset color
];

export function readAnything(img: ImageData, x: number, y: number) {
	let reader = new RightClickReader();
	if (reader.find(new ImgRefData(img))) {
		let menu = reader.read(img);
		return { type: "rightclick", line: menu.hoveredText, menu } as RightClickResult;
	}

	let col = OCR.getChatColor(img, new Rect(x - 10, y - 7, 20, 7), defaultcolors);
	if (col) {
		for (let font of chatfonts) {
			let text11pt = OCR.findReadLine(img, font.font, [col], x, y);
			let m = text11pt.text.match(/\w/g);
			//match at least 3 word characters efore we accept it
			if (m && m.length >= 3)
				return { type: "text", font: font.name, line: text11pt } as TextResult;
		}
	}

	//TODO other fonts

	//TODO other alt1+1-able things


	return null
}