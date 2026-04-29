export const manifest = (() => {
function __memo(fn) {
	let value;
	return () => value ??= (value = fn());
}

return {
	appDir: "_app",
	appPath: "_app",
	assets: new Set([]),
	mimeTypes: {},
	_: {
		client: {start:"_app/immutable/entry/start.CdWq6zwl.js",app:"_app/immutable/entry/app.aT27YTxe.js",imports:["_app/immutable/entry/start.CdWq6zwl.js","_app/immutable/chunks/CdGYy9iB.js","_app/immutable/chunks/C2Fv0eq0.js","_app/immutable/chunks/BHl4wu82.js","_app/immutable/entry/app.aT27YTxe.js","_app/immutable/chunks/C2Fv0eq0.js","_app/immutable/chunks/BgOpmHos.js","_app/immutable/chunks/CA10LYTd.js","_app/immutable/chunks/BHl4wu82.js","_app/immutable/chunks/DMC_SIBG.js"],stylesheets:[],fonts:[],uses_env_dynamic_public:false},
		nodes: [
			__memo(() => import('./nodes/0.js')),
			__memo(() => import('./nodes/1.js')),
			__memo(() => import('./nodes/2.js'))
		],
		remotes: {
			
		},
		routes: [
			{
				id: "/",
				pattern: /^\/$/,
				params: [],
				page: { layouts: [0,], errors: [1,], leaf: 2 },
				endpoint: null
			}
		],
		prerendered_routes: new Set([]),
		matchers: async () => {
			
			return {  };
		},
		server_assets: {}
	}
}
})();
