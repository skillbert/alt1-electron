{
	"variables": {
		"pkg-config": "pkg-config"
	},
	"targets": [
		{
			"target_name": "addon",
			"sources": [
				"./native/lib.cc",
				"./native/util.cc",
				"./native/os_x11_linux.cc",
			],
			"include_dirs": [
				"<!@(node -p \"require('node-addon-api').include\")"
			],
			"dependencies": [
				"<!(node -p \"require('node-addon-api').gyp\")"
			],
			"cflags!": [ "-fno-exceptions" ],
			"cflags_cc!": [ "-fno-exceptions" ],
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
					]
				}],
				['OS=="linux"', {
					"defines": [
						'OS_LINUX',
					],
					'cflags': [
						'<!@(<(pkg-config) --cflags xcb)',
						'<!@(<(pkg-config) --cflags xcb-ewmh)',
						'<!@(<(pkg-config) --cflags xcb-shm)',
						'<!@(<(pkg-config) --cflags xcb-composite)',
						'<!@(<(pkg-config) --cflags libprocps)'
					],
					'ldflags': [
						'<!@(<(pkg-config) --libs-only-L --libs-only-other xcb)',
						'<!@(<(pkg-config) --libs-only-L --libs-only-other xcb-ewmh)',
						'<!@(<(pkg-config) --libs-only-L --libs-only-other xcb-shm)',
						'<!@(<(pkg-config) --libs-only-L --libs-only-other xcb-composite)',
						'<!@(<(pkg-config) --libs-only-L --libs-only-other libprocps)'
					],
					'libraries': [
						'<!@(<(pkg-config) --libs-only-l xcb)',
						'<!@(<(pkg-config) --libs-only-l xcb-ewmh)',
						'<!@(<(pkg-config) --libs-only-l xcb-shm)',
						'<!@(<(pkg-config) --libs-only-l xcb-composite)',
						'<!@(<(pkg-config) --libs-only-l libprocps)'
					],
					"cflags_cc": [ "-std=c++17" ],
				}],
			]
		}
	]
}
