

namespace OpenGLCapture
{
	struct HookedProcess;

	struct ImageData
	{
		int width;
		int height;
		byte* data;
		void* reserved;
	};

	struct Point
	{
		int x;
		int y;
	};


	typedef int (*FnGetDebug)(char* str, int len);
	typedef HookedProcess* (*FnHookProcess)(HWND hwnd);
	typedef byte* (*FnCaptureArea)(HookedProcess* hook, JSRectangle rect);
	typedef byte* (*FnCaptureMultiple)(HookedProcess* hook, JSRectangle* rects, int number);
	typedef void (*FnUnhookProcess)(HookedProcess* hook);
	typedef void (*FnSetTickTime)(HookedProcess* hook, int time);
	typedef int (*FnGetFrameTime)(HookedProcess* hook);

	//image acceleration
	typedef void (*FnFindSubImg)(Point** outlist, int* outlength, ImageData* haystack, ImageData* needle, JSRectangle area, int maxd);
	typedef int (*FnCpuFeatures)();

	FnGetDebug GetDebug;
	FnHookProcess HookProcess;
	FnCaptureArea CaptureArea;
	FnCaptureMultiple CaptureMultiple;
	FnUnhookProcess UnhookProcess;
	FnSetTickTime SetTickTime;
	FnGetFrameTime GetFrameTime;

	//needs 16 byte allignment!!!!
	FnFindSubImg FindSubImg;
	FnCpuFeatures CpuFeatures;


	void init() {

		auto qq = LoadLibraryA;
		auto qw = GetProcAddress;
		//TODO whats wrong with relative paths here
		auto ezhdll = LoadLibraryA("Alt1Native.dll");
		auto hdll = LoadLibraryA("C:/runeapps/alt1lite/dist/Alt1Native.dll");
		auto qqweq = LoadLibraryA;
		GetDebug = (FnGetDebug)GetProcAddress(hdll, "GetDebug");
		HookProcess = (FnHookProcess)GetProcAddress(hdll, "HookProcess");
		CaptureArea = (FnCaptureArea)GetProcAddress(hdll, "CaptureArea");
		CaptureMultiple = (FnCaptureMultiple)GetProcAddress(hdll, "CaptureMultiple");
		UnhookProcess = (FnUnhookProcess)GetProcAddress(hdll, "UnhookProcess");
		SetTickTime = (FnSetTickTime)GetProcAddress(hdll, "SetTickTime");
		GetFrameTime = (FnGetFrameTime)GetProcAddress(hdll, "GetFrameTime");

		//needs 16 byte allignment!!!!
		FindSubImg = (FnFindSubImg)GetProcAddress(hdll, "FindSubImg");
		CpuFeatures = (FnCpuFeatures)GetProcAddress(hdll, "CpuFeatures");
	}
}// namespace OpenGLCapture