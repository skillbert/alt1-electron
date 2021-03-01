import { readAnything } from "../readers/alt1reader";


let tests = [
	{ img: "followplayer", expected: "Follow DannyCOYS (level: 138)" },
	{ img: "examine1", expected: "Examine Altar of War" },
	{ img: "examine2", expected: "Examine Archaeology journal" },
	{ img: "chat11pt1", expected: "] Attempting to join channel..." },
	{ img: "chat11pt2", expected: "News: Micheldy has just achieved 120 Slayer!" },
	{ img: "chat13pt1", expected: "News: Lawdogg21 has achieved 200 million XP in Farming!" },
	{ img: "chat15pt1", expected: "News: Mad Merlin has just achieved 120 Slayer!" },
	//TODO this one suffers from a rs bug at 17pt, spaces in player names are 2px shorter
	{ img: "chat17pt1", expected: "Merlin has just achieved 120 Slayer!" }
];

export default (async function () {
	console.log("starting alt1pressed tests");
	for (let test of tests) {
		let img: ImageData = await require(`./alt1pressedimgs/${test.img}.data.png`);
		img.show();
		let res = readAnything(img, Math.round(img.width / 2), Math.round(img.height / 2));
		let restext = res?.line.text;
		console.log(`== ${test.img} ==\nexpected: ${test.expected}\nread:     ${restext}`);
	}
})();