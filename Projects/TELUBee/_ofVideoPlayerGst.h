#ifndef _OF_VIDEO_PLAYER_GST
#define _OF_VIDEO_PLAYER_GST

//#include "ofConstants.h"
//#include "ofTexture.h"
#include "ofGstUtilsWin32.h"
#include "ImageInfo.h"
//#include "ofConstants.h"

#define OF_VIDEO_PLAYER_GSTREAMER
#undef OF_VIDEO_PLAYER_QUICKTIME

#ifdef OF_VIDEO_PLAYER_GSTREAMER
//	#include "ofGstUtilsWin32.h"
#else
#include "ofQtUtils.h"
#endif


//
//#undef TARGET_WIN32
//#define TARGET_LINUX

//---------------------------------------------

class ofVideoPlayerGst 
{

	public:


		ofVideoPlayerGst ();
		virtual ~ofVideoPlayerGst();

		bool 				loadMovie(core::string name);
		bool				Connect(const core::string& ip,int remotePort,int localPort);
		bool				IsConnected(){return bLoaded;}
		void 				closeMovie();
		void 				close();

		void				update();			//same as idleMovie
		void 				idleMovie();		// rename to updateMovie?
		void 				play();
		void 				stop();

		int 				width, height;
		float  				speed;
		bool 				bLoaded;

		bool 				isFrameNew();
		unsigned char * 	getPixels();
		float 				getPosition();
		float 				getSpeed();
		float 				getDuration();
		bool				getIsMovieDone();

		void 				setPosition(float pct);
		void 				setVolume(int volume);
		void 				setLoopState(int state);
		void   				setSpeed(float speed);
		void				setFrame(int frame);  // frame 0 = first frame...

		void 				setUseTexture(bool bUse);
		video::ImageInfo*			getTextureReference();
		void 				draw(float x, float y, float w, float h);
		void 				draw(float x, float y);

		//the anchor is the point the image is drawn around.
		//this can be useful if you want to rotate an image around a particular point.
        void				setAnchorPercent(float xPct, float yPct);	//set the anchor as a percentage of the image width/height ( 0.0-1.0 range )
        void				setAnchorPoint(int x, int y);				//set the anchor point in pixels
        void				resetAnchor();								//resets the anchor to (0, 0)

		void 				setPaused(bool bPause);

		int					getCurrentFrame();
		int					getTotalNumFrames();

		void				firstFrame();
		void				nextFrame();
		void				previousFrame();

		float 				getHeight();
		float 				getWidth();

		//--------------------------------------
		#ifdef OF_VIDEO_PLAYER_QUICKTIME
		//--------------------------------------
			MovieDrawingCompleteUPP myDrawCompleteProc;
			MovieController  	thePlayer;
			GWorldPtr 			offscreenGWorld;
			Movie 			 	moviePtr;
			unsigned char * 	offscreenGWorldPixels;	// 32 bit: argb (qt k32ARGBPixelFormat)
			void				qtGetFrameCount(Movie & movForcount);
		//--------------------------------------
		#endif
		//--------------------------------------

		//--------------------------------------
		#ifdef OF_VIDEO_PLAYER_GSTREAMER
		//--------------------------------------
		ofGstUtils 			gstUtils;
		//--------------------------------------
		#endif
		//--------------------------------------

		int					nFrames;				// number of frames
		unsigned char * 	pixels;					// 24 bit: rgb
		bool 				bHavePixelsChanged;
		video::ImageInfo		tex;					// a ptr to the texture we are utilizing
		bool 				bUseTexture;			// are we using a texture
		bool				allocated;				// so we know to free pixels or not

	protected:


		void 				start();
		void 				createImgMemAndGWorld();
		bool 				bStarted;
		bool 				bPlaying;
		bool 				bPaused;
		bool 				bIsFrameNew;			// if we are new



};
#endif


//#define TARGET_WIN32
//#undef TARGET_LINUX
#undef OF_VIDEO_PLAYER_GSTREAMER
#define OF_VIDEO_PLAYER_QUICKTIME


