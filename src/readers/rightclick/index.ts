import { ImageData, ImgRef, Rect, RectLike } from "alt1";
import { ImageDetect } from "alt1";
import * as OCR from "alt1/ocr";

let rightclickfont = require("./imgs/rightclick.fontmeta.json");

let imgs = ImageDetect.webpackImages({
	topleft: require("./imgs/topleft.data.png"),
	botleft: require("./imgs/botleft.data.png"),
	topright: require("./imgs/topright.data.png")
});

export function mapRightclickColorSpace(src: ImageData, rect: RectLike) {
	let dest = new ImageData(rect.width, rect.height);
	let srcdata = src.data;
	let destdata = dest.data;
	for (let dy = 0; dy < rect.height; dy++) {
		for (let dx = 0; dx < rect.width; dx++) {
			let isrc = (dy + rect.y) * src.width * 4 + (dx + rect.x) * 4;
			let idest = dy * dest.width * 4 + dx * 4;
			let hoverdiff = Math.abs(srcdata[isrc + 0] - 40) + Math.abs(srcdata[isrc + 1] - 89) + Math.abs(srcdata[isrc + 2] - 112);
			let bgdiff = Math.abs(srcdata[isrc + 0] - 10) + Math.abs(srcdata[isrc + 1] - 29) + Math.abs(srcdata[isrc + 2] - 38);
			let blackdiff = srcdata[isrc + 0] + srcdata[isrc + 1] + srcdata[isrc + 2];
			let col = (blackdiff <= 20 ? 0 : hoverdiff < 20 || bgdiff < 20 ? 128 : 255);
			destdata[idest + 0] = col;
			destdata[idest + 1] = col;
			destdata[idest + 2] = col;
			destdata[idest + 3] = 255;
		}
	}
	return dest;
}

export default class RightClickReader {
	pos: RectLike | null = null;
	find(img: ImgRef) {
		let locs = img.findSubimage(imgs.topleft);
		if (locs.length == 0) { return null; }
		let topleft = locs[0];
		let topright = img.findSubimage(imgs.topright, topleft.x, topleft.y, img.width - topleft.x, imgs.topright.height);
		let botleft = img.findSubimage(imgs.botleft, topleft.x, topleft.y, imgs.botleft.width, img.height - topleft.y);
		if (topright.length == 0 || botleft.length == 0) { return null; }
		this.pos = {
			x: topleft.x,
			y: topleft.y - 1,
			width: topright[0].x - topleft.x + 3,
			height: botleft[0].y - topleft.y + 9
		};
		return this.pos;
	}
	read(buf: ImageData) {
		if (!this.pos) { throw new Error("not found yet"); }
		const lineheight = 16;
		let nlines = Math.round((this.pos.height - 18 - 3) / lineheight);
		let line0y = this.pos.y + 18;
		let linex = this.pos.x + 3;
		let hoveredText: ReturnType<typeof OCR.readLine> | null = null;
		for (let a = 0; a < nlines; a++) {
			let liney = line0y + lineheight * a;
			let hoverlum = buf.getPixelValueSum(linex, liney);
			if (hoverlum > 160) {
				//TODO get better way to deal with color detection instead
				let subbuf = mapRightclickColorSpace(buf, new Rect(linex, liney, this.pos.width - 6, 19));
				hoveredText = OCR.readLine(subbuf, rightclickfont, [255, 255, 255], 0, 12, true);
			}
		}
		return { hoveredText, nlines };
	}
}