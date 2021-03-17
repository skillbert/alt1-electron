{
	"variables": {
		"pkg-config": "pkg-config"
	},
	"targets": [
		{
			"target_name": "addon",
			"sources": [
				"./native/lib.cc",
				"./native/util.cc"
			],
			"include_dirs": [
				"<!@(node -p \"require('node-addon-api').include\")"
			],
			"dependencies": [
				"<!(node -p \"require('node-addon-api').gyp\")"
			],
			"cflags!": ["-fno-exceptions"],
			"cflags_cc!": ["-fno-exceptions"],
			"xcode_settings": {
				"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
				"CLANG_CXX_LIBRARY": "libc++",
				"MACOSX_DEPLOYMENT_TARGET": "10.7"
			},
			"msvs_settings": {
				"VCCLCompilerTool": { "ExceptionHandling": 1 },
			},
			"defines": [
				"NAPI_CPP_EXCEPTIONS"
			],
			"conditions": [
				['OS=="win"', {
					"defines": [
						'OS_WIN',
						'OPENGL_SUPPORTED',
					],
					"sources": [
						"./native/os_win.cc"
					],
					"libraries": ["<(module_root_dir)/libs/Alt1Native.lib"],
					"copies": [
						{
							"destination": "<(module_root_dir)/dist/",
							"files": ["<(module_root_dir)/libs/InjectDLL64.dll"]
						}, {
							"destination": "<(PRODUCT_DIR)/",
							"files": ["<(module_root_dir)/libs/Alt1Native.dll"]
						}
					]
				}],
				['OS=="linux"', {
					"defines": [
						'OS_LINUX',
					],
					"sources": [
						"./native/os_x11_linux.cc",
						"./native/linux/x11.cc",
						"./native/linux/x11_event.cc",
						"./native/linux/shm.cc"
					],
					'cflags': [
						'<!@(<(pkg-config) --cflags xcb xcb-ewmh xcb-shm xcb-composite libprocps)'
					],
					'ldflags': [
						'<!@(<(pkg-config) --libs-only-L --libs-only-other xcb xcb-ewmh xcb-shm xcb-composite libprocps)'
					],
					'libraries': [
						'<!@(<(pkg-config) --libs-only-l xcb xcb-ewmh xcb-shm xcb-composite libprocps)'
					],
					"cflags_cc": [ "-std=c++17" ],
				}],
				['OS=="mac"', {
					"defines": [
						'OS_MAC',
					],
					"sources": [
						"./native/os_mac.mm"
					]
				}],
			]
		}
	]
}
