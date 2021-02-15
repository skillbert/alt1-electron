import { EventEmitter } from "events";

type constr = {
	new <MAP extends { [key: string]: any[] }>(): typed<MAP>
}

interface typed<MAP extends { [key: string]: any[] }> extends EventEmitter {
	addListener<K extends keyof MAP>(event: K, listener: (...args: MAP[K]) => void): this;
	on<K extends keyof MAP>(event: K, listener: (...args: MAP[K]) => void): this;
	once<K extends keyof MAP>(event: K, listener: (...args: MAP[K]) => void): this;
	removeListener<K extends keyof MAP>(event: K, listener: (...args: MAP[K]) => void): this;
	off<K extends keyof MAP>(event: K, listener: (...args: MAP[K]) => void): this;
	emit<K extends keyof MAP>(event: K, ...args: MAP[K]): boolean;

	//symbol keyed to make ts happy
	addListener(event: symbol, listener: (...args: any[]) => void): this;
	on(event: symbol, listener: (...args: any[]) => void): this;
	once(event: symbol, listener: (...args: any[]) => void): this;
	removeListener(event: symbol, listener: (...args: any[]) => void): this;
	off(event: symbol, listener: (...args: any[]) => void): this;
	emit(event: symbol, ...args: any[]): boolean;
}

export var TypedEmitter = EventEmitter as constr;