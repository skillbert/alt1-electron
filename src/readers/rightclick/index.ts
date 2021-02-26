import { ImgRef, PointLike, RectLike } from "@alt1/base";
import { ImageDetect } from "@alt1/base";

let imgs = ImageDetect.webpackImages({
	topleft: require("./imgs/topleft.data.png"),
	botleft: require("./imgs/botleft.data.png"),
	topright: require("./imgs/topright.data.png")
});


export default class RightClickReader {
	pos: RectLike | null = null;
	find(img: ImgRef) {
		let locs = img.findSubimage(imgs.topleft);
		if (locs.length == 0) { return null; }
		let topleft = locs[0];
		let topright = img.findSubimage(imgs.topright, topleft.x, topleft.y, img.width, imgs.topright.height);
		let botleft = img.findSubimage(imgs.botleft, topleft.x, topleft.y, imgs.botleft.width, img.height);
		if (topright.length == 0 || botleft.length == 0) { return null; }
		this.pos = {
			x: topleft.x,
			y: topleft.y,
			width: topright[0].x - topleft.x,
			height: botleft[0].y - topleft.y
		};
		return this.pos;
	}
}