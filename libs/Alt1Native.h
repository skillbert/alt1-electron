
namespace Alt1Native {
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

	extern "C" {
		int GetDebug(char* str, int len);
#ifdef OS_WIN
		HookedProcess* HookProcess(HWND hwnd);
#endif
		byte* CaptureArea(HookedProcess* hook, JSRectangle rect);
		byte* CaptureMultiple(HookedProcess* hook, JSRectangle* rects, int number);
		void UnhookProcess(HookedProcess* hook);
		void SetTickTime(HookedProcess* hook, int time);
		int GetFrameTime(HookedProcess* hook);

		//image acceleration
		//needs 16 byte allignment!!!!
		void FindSubImg(Point** outlist, int* outlength, ImageData* haystack, ImageData* needle, JSRectangle area, int maxd);
		int CpuFeatures();
	}
}
