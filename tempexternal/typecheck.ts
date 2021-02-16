////////////////////////////////////////////////
//// COPIED FROM RUNEAPPS REPO DO NOT EDIT /////
////////////////////////////////////////////////



export type UservarType<T extends Uservar<any, any>> = T extends Uservar<infer Q, any> ? Q : never;
export type UservarSerialType<T extends Uservar<any, any>> = T extends Uservar<any, infer Q> ? Q : never;
export type ObjUservar<T extends { [key: string]: Uservar<any, any> }> = { props: T } &
	Uservar<{ [key in keyof T]: UservarType<T[key]> }, { [key in keyof T]: UservarSerialType<T[key]> }>
export type LazyCheckResult<T> = { load: () => T, async: () => Promise<T> }

export class UservarError extends Error { }

export type UservarMeta = {
	name?: string;
};

type UservarOpts = { defaultOnError?: boolean };

export type Uservar<T, SER = T> = {
	parent?: Uservar<T, SER>,
	type: Function,
	default(): T,
	load(v: unknown, opts?: UservarOpts): T,
	store(v: T): SER,
};
export function loadJsonOrDefault<T extends Uservar<any, any>>(meta: T, v: unknown) {
	try {
		if (typeof v != "string") { throw new Error("input is not of type string"); }
		return meta.load(JSON.parse(v));
	} catch (e) {
		console.log("Error while loading user var from json: " + e);
		return meta.default();
	}
}

export function loadOrDefault<T extends Uservar<any, any>>(meta: T, v: unknown) {
	try {
		return meta.load(v);
	} catch (e) {
		console.log("Error while loading user var: " + e);
		return meta.default();
	}
}

export function storageSetting<T>(name: string, t: Uservar<T, any>) {
	let r = {
		load() {
			let v = localStorage[name];
			var usedjson = false;
			try {
				var jsonv = JSON.parse(v);
				usedjson = true;
			} catch (e) { }
			let r: T;
			try {
				r = t.load(usedjson ? jsonv : v, { defaultOnError: true })
			} catch (e) {
				r = t.default();
			}
			return r;
		},
		save(v: T) {
			localStorage[name] = JSON.stringify(t.store(v));
		},
		meta: t
	}
	return r;
}

export function loadSubVar<T extends Uservar<any, any>>(meta: T, v: unknown, opts: UservarOpts | undefined) {
	if (opts?.defaultOnError) {
		try { return meta.load(v, opts); }
		catch (e) { return meta.default(); }
	}
	return meta.load(v, opts);
}

namespace Checks {
	export function opt<T extends Uservar<any, any>>(prop: T) {
		var r: { prop: T } & Uservar<UservarType<T> | null | undefined, UservarSerialType<T> | null | undefined> = {
			type: opt,
			prop: prop,
			default() { return null; },
			load(v, opts) {
				if (typeof v == "undefined" || v == null) { return null; }
				return loadSubVar(prop, v, opts);
			},
			store(v) {
				if (typeof v == "undefined" || v == null) { return null; }
				return prop.store(v);
			}
		};
		return r;
	}

	export function num(d = 0, min?: number, max?: number) {
		var r: Uservar<number> & { min: number | undefined, max: number | undefined } = {
			type: num,
			max: max,
			min: min,
			default() { return d; },
			load(v) {
				if (typeof v != "number") { throw new UservarError("number expected"); }
				if (typeof r.max != "undefined" && v > r.max) { throw new UservarError(`number expected to be lower or equal to ${r.max}`); }
				if (typeof r.min != "undefined" && v < r.min) { throw new UservarError(`number expected to be lower or equal to ${r.min}`); }
				return v;
			},
			store(v) { return v; }
		};
		return r;
	}

	export function int(d = 0, min?: number, max?: number) {
		let inner = num(d, min, max);
		var r = augment(int, inner, {
			load: (v: unknown) => {
				var w = inner.load(v);
				if ((w | 0) != w) { throw new UservarError("integer expected"); }
				return w;
			}
		});
		return r;
	}

	export function strenum<T extends { [key: string]: string }>(opts: T, d?: keyof T) {
		if (typeof d == "undefined") {
			d = Object.keys(opts)[0] as keyof T;
		}
		var r: Uservar<keyof T> & { opts: T } = {
			opts: opts,
			default() { return d!; },
			load(v) {
				if (typeof v != "string") { throw new UservarError("strenum should be of type string"); }
				if (Object.keys(opts).indexOf(v) == -1) { throw new UservarError("strenum is not part of predefined enum values"); }
				return v as keyof T
			},
			store(v) { return v; },
			type: strenum,
		}
		return r;
	}

	export function bool(d = false) {
		var r: Uservar<boolean> = {
			type: bool,
			default() { return d; },
			load(v) {
				if (typeof v != "boolean") { throw new UservarError("boolean expected"); }
				return v;
			},
			store(v) {
				return v;
			}
		}
		return r;
	}

	export function str(d = "", maxlength?: number, matcher?: RegExp) {
		var r: Uservar<string> & { maxlength: number | undefined, matcher: RegExp | undefined } = {
			type: str,
			maxlength: maxlength,
			matcher: matcher,
			default() { return d; },
			load(v) {
				if (typeof v != "string") { throw new UservarError("string expected"); }
				if (typeof r.maxlength != "undefined" && v.length > r.maxlength) { throw new UservarError(`string length longer than max length ${r.maxlength}`); }
				if (typeof r.matcher != "undefined" && !r.matcher.exec(v)) { throw new UservarError(`string did not match regex ${r.matcher}`); }
				return v;
			},
			store(v) { return v; }
		};
		return r;
	}

	export function arr<T extends Uservar<any, any>>(prop: T, maxlength?: number) {
		var r: { prop: T, maxlength: number | undefined } & Uservar<UservarType<T>[]> = {
			type: arr,
			prop: prop,
			maxlength: maxlength,
			default: () => [],
			load: (v, opts) => {
				if (!Array.isArray(v)) { throw new UservarError("Array expected"); }
				if (typeof r.maxlength != "undefined" && v.length > r.maxlength) { throw new UservarError(`Array length expected to be equal to or lower than ${r.maxlength}`); }

				var rarr: UservarType<T>[] = [];
				for (let a = 0; a < v.length; a++) {
					rarr[a] = loadSubVar(prop, v[a], opts);
				}
				return rarr;
			},
			store: (v) => {
				return v.map(el => prop.store(el));
			}
		}
		return r;
	}

	export function obj<T extends { [key: string]: Uservar<any, any> }>(props: T) {
		var r: ObjUservar<T> = {
			type: obj,
			props: props,
			default: () => {
				var r = Object.create(null);
				for (let prop in props) { r[prop] = props[prop].default(); }
				return r;
			},
			load: (v, opts) => {
				if (typeof v != "object" || !v) { throw new UservarError("object expected"); }
				var robj = Object.create(null);
				for (let prop in props) {
					var propv = v[prop as any];
					robj[prop] = loadSubVar(props[prop], propv, opts);
				}
				return robj;
			},
			store: (v) => {
				var r = Object.create(null);
				for (let prop in props) {
					r[prop] = props[prop].store(v[prop]);
				}
				return r;
			}
		};
		return r;
	}

	export function map<T extends Uservar<any, any>>(prop: T, maxentries?: number, maxkeylength?: number) {
		var r: { prop: T, maxentries: number | undefined, maxkeylength: number | undefined } & Uservar<{ [key: string]: UservarType<T> }> = {
			type: map,
			prop: prop,
			maxentries: maxentries,
			maxkeylength: maxkeylength,
			default: () => Object.create(null),
			load: (v, opts) => {
				if (typeof v != "object" || !v) { throw new UservarError("Object expected"); }
				if (typeof r.maxentries != "undefined" && Object.keys(v).length > r.maxentries) { throw new UservarError(`number of keys expected to be equal to or lower than ${r.maxentries}`); }

				var robj: { [key: string]: UservarType<T> } = Object.create(null);
				for (let key in v) {
					if (typeof r.maxkeylength != "undefined" && key.length > r.maxkeylength) { throw new UservarError(`key of map expected to be shorter than ${r.maxkeylength}`); }
					var propv = v[key as any];
					robj[key] = loadSubVar(prop, propv, opts);
				}
				return robj;
			},
			store: (v) => {
				var r = Object.create(null);
				for (let key in v) {
					r[key] = prop.store(v[key]);
				}
				return r;
			}
		}
		return r;
	}

	export function unsafejson() {
		var r: Uservar<unknown, unknown> = {
			type: map,
			default: () => null,
			load: (v) => v,
			store: (v) => v
		};
		return r;
	}

	export function augment<T extends Uservar<any, any>, Q>(newtype: Function, old: T, newprops?: Q) {
		let rr: T = Object.create(old);
		rr.parent = old;
		rr.type = newtype;
		Object.assign(rr, newprops);
		return rr as T & Q;
	}
}

export default Checks;