{
	"targets": [
		{
			"target_name": "addon",
			"sources": [
				"./native/os_x11_linux.cc",
				"./native/lib.cc",
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
					"cflags_cc": [ "-std=c++17" ],
				}],
			]
		}
	]
}
