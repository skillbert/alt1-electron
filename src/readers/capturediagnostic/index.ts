import * as a1lib from "alt1";

let imgs = a1lib.ImageDetect.webpackImages({
    homeport: require("./imgs/homeport.data.png")
});

export function runCaptureDiagnostic(imgref: a1lib.ImgRef) {
    let homeport = imgref.findSubimage(imgs.homeport);

    return {
        homeportfound: homeport.length == 1
    }
}