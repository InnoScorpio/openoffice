/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_vcl.hxx"

#include <stdio.h>

#include "tools/fsys.hxx"
#include "tools/getprocessworkingdir.hxx"
#include <tools/solarmutex.hxx>

#include "osl/process.h"

#include "rtl/ustrbuf.hxx"

#include "vcl/svapp.hxx"
#include "vcl/window.hxx"
#include "vcl/timer.hxx"

#include "aqua/saldata.hxx"
#include "aqua/salinst.h"
#include "aqua/salframe.h"
#include "aqua/salobj.h"
#include "aqua/salsys.h"
#include "aqua/salvd.h"
#include "aqua/salbmp.h"
#include "aqua/salprn.h"
#include "aqua/saltimer.h"
#include "aqua/vclnsapp.h"

#include "print.h"
#include "impbmp.hxx"
#include "salimestatus.hxx"

#include <comphelper/processfactory.hxx>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/uri/XExternalUriReferenceTranslator.hpp>
#include <com/sun/star/uri/ExternalUriReferenceTranslator.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>

#include "premac.h"
#include <Foundation/Foundation.h>
#include <ApplicationServices/ApplicationServices.h>
#import "apple_remote/RemoteMainController.h"
#include "apple_remote/RemoteControl.h"
#include "postmac.h"

using namespace std;
using namespace ::com::sun::star;

extern sal_Bool ImplSVMain();

static sal_Bool* gpbInit = 0;
static NSMenu* pDockMenu = nil;
static bool bNoSVMain = true;
static bool bLeftMain = false;
// -----------------------------------------------------------------------

class AquaDelayedSettingsChanged : public Timer
{
    bool            mbInvalidate;
    public:
    AquaDelayedSettingsChanged( bool bInvalidate ) :
        mbInvalidate( bInvalidate )
    {
    }
    
    virtual void Timeout()
    {
        SalData* pSalData = GetSalData();
        if( ! pSalData->maFrames.empty() )
            pSalData->maFrames.front()->CallCallback( SALEVENT_SETTINGSCHANGED, NULL );
        
        if( mbInvalidate )
        {
            for( std::list< AquaSalFrame* >::iterator it = pSalData->maFrames.begin();
                it != pSalData->maFrames.end(); ++it )
            {
                if( (*it)->mbShown )
                    (*it)->SendPaintEvent( NULL );
            }
        }
        Stop();
        delete this;                                        
    }
};

void AquaSalInstance::delayedSettingsChanged( bool bInvalidate )
{
    vos::OGuard aGuard( *mpSalYieldMutex );
    AquaDelayedSettingsChanged* pTimer = new AquaDelayedSettingsChanged( bInvalidate );
    pTimer->SetTimeout( 50 );
    pTimer->Start();
}


// the AppEventList must be available before any SalData/SalInst/etc. objects are ready
typedef std::list<const ApplicationEvent*> AppEventList;
AppEventList AquaSalInstance::aAppEventList;

NSMenu* AquaSalInstance::GetDynamicDockMenu()
{
    if( ! pDockMenu && ! bLeftMain )
        pDockMenu = [[NSMenu alloc] initWithTitle: @""];
    return pDockMenu;
}

bool AquaSalInstance::isOnCommandLine( const rtl::OUString& rArg )
{
    sal_uInt32 nArgs = osl_getCommandArgCount();
    for( sal_uInt32 i = 0; i < nArgs; i++ )
    {
        rtl::OUString aArg;
        osl_getCommandArg( i, &aArg.pData );
        if( aArg.equals( rArg ) )
            return true;
    }
    return false;
}
 

// initialize the cocoa VCL_NSApplication object
// returns an NSAutoreleasePool that must be released when the event loop begins
static void initNSApp()
{
    // create our cocoa NSApplication
    [VCL_NSApplication sharedApplication];
    
    SalData::ensureThreadAutoreleasePool();

    // put cocoa into multithreaded mode
    [NSThread detachNewThreadSelector:@selector(enableCocoaThreads:) toTarget:[[CocoaThreadEnabler alloc] init] withObject:nil];

    // activate our delegate methods
    [NSApp setDelegate: NSApp];
    
    [[NSNotificationCenter defaultCenter] addObserver: NSApp
                                          selector: @selector(systemColorsChanged:)
                                          name: NSSystemColorsDidChangeNotification
                                          object: nil ];
    [[NSNotificationCenter defaultCenter] addObserver: NSApp
                                          selector: @selector(screenParametersChanged:)
                                          name: NSApplicationDidChangeScreenParametersNotification
                                          object: nil ];
    // add observers for some settings changes that affect vcl's settings
    // scrollbar variant
    [[NSDistributedNotificationCenter defaultCenter] addObserver: NSApp
                                          selector: @selector(scrollbarVariantChanged:)
                                          name: @"AppleAquaScrollBarVariantChanged"
                                          object: nil ];
    // scrollbar page behavior ("jump to here" or not)
    [[NSDistributedNotificationCenter defaultCenter] addObserver: NSApp
                                          selector: @selector(scrollbarSettingsChanged:)
                                          name: @"AppleNoRedisplayAppearancePreferenceChanged"
                                          object: nil ];

    // get System Version and store the value in GetSalData()->mnSystemVersion
    SInt32 systemVersion = OSX_VER_LION; // initialize with the minimal requirement
    const OSErr err = Gestalt( gestaltSystemVersion, &systemVersion);
    if( err == noErr ) 
    {
        GetSalData()->mnSystemVersion = systemVersion;
#if OSL_DEBUG_LEVEL > 1
        fprintf( stderr, "OSX System Version 0x%04x\n", (unsigned int)systemVersion);
#endif
    }
    else
        NSLog(@"Unable to obtain system version: %ld", (long)err);

    GetSalData()->mpAppleRemoteMainController = [[AppleRemoteMainController alloc] init];

    [[NSDistributedNotificationCenter defaultCenter] addObserver: NSApp
                                           selector: @selector(applicationWillBecomeActive:)
                                           name: @"AppleRemoteWillBecomeActive"
                                           object: nil ];
                                         
    [[NSDistributedNotificationCenter defaultCenter] addObserver: NSApp
                                           selector: @selector(applicationWillResignActive:)
                                           name: @"AppleRemoteWillResignActive"
                                           object: nil ];

    if( ImplGetSVData()->mbIsTestTool )
        [NSApp activateIgnoringOtherApps: YES];
}

sal_Bool ImplSVMainHook( sal_Bool * pbInit )
{
	gpbInit = pbInit;

	bNoSVMain = false;
	initNSApp();

        rtl::OUString aExeURL, aExe;
        osl_getExecutableFile( &aExeURL.pData );
        osl_getSystemPathFromFileURL( aExeURL.pData, &aExe.pData );
        rtl::OString aByteExe( rtl::OUStringToOString( aExe, osl_getThreadTextEncoding() ) );
        
#ifdef DEBUG
        aByteExe += OString ( " NSAccessibilityDebugLogLevel 1" );
        const char* pArgv[] = { aByteExe.getStr(), NULL };
        NSApplicationMain( 3, pArgv );
#else
        const char* pArgv[] = { aByteExe.getStr(), NULL };
        NSApplicationMain( 1, pArgv );
#endif

    return TRUE;   // indicate that ImplSVMainHook is implemented
}

// =======================================================================

void SalAbort( const XubString& rErrorText )
{
	if( !rErrorText.Len() )
		fprintf( stderr, "Application Error " );
	else
		fprintf( stderr, "%s ",
			ByteString( rErrorText, gsl_getSystemTextEncoding() ).GetBuffer() );
	abort();
}

// -----------------------------------------------------------------------

void InitSalData()
{
	SalData *pSalData = new SalData;
	SetSalData( pSalData );
}

// -----------------------------------------------------------------------

const ::rtl::OUString& SalGetDesktopEnvironment()
{
    static OUString aDesktopEnvironment(RTL_CONSTASCII_USTRINGPARAM( "MacOSX" ));
    return aDesktopEnvironment;
}

// -----------------------------------------------------------------------

void DeInitSalData()
{
	SalData *pSalData = GetSalData();
    if( pSalData->mpStatusItem )
    {
        [pSalData->mpStatusItem release];
        pSalData->mpStatusItem = nil;
    }
	delete pSalData;
	SetSalData( NULL );
}

// -----------------------------------------------------------------------

extern "C" {
#include <crt_externs.h>
}

// -----------------------------------------------------------------------

void InitSalMain()
{
    rtl::OUString urlWorkDir;
    rtl_uString *sysWorkDir = NULL;
    if (tools::getProcessWorkingDir(&urlWorkDir))
    {
        oslFileError err2 = osl_getSystemPathFromFileURL(urlWorkDir.pData, &sysWorkDir);
        if (err2 == osl_File_E_None)
        {
            ByteString aPath( getenv( "PATH" ) );
            ByteString aResPath( getenv( "STAR_RESOURCEPATH" ) );
            ByteString aLibPath( getenv( "DYLD_LIBRARY_PATH" ) );
            ByteString aCmdPath( OUStringToOString(OUString(sysWorkDir), RTL_TEXTENCODING_UTF8).getStr() );
            ByteString aTmpPath;
            // Get absolute path of command's directory
            if ( aCmdPath.Len() ) {
                DirEntry aCmdDirEntry( aCmdPath );
                aCmdDirEntry.ToAbs();
                aCmdPath = ByteString( aCmdDirEntry.GetPath().GetFull(), RTL_TEXTENCODING_ASCII_US );
            }
            // Assign to PATH environment variable
            if ( aCmdPath.Len() )
            {
                aTmpPath = aCmdPath;
                if ( aPath.Len() )
                    aTmpPath += ByteString( DirEntry::GetSearchDelimiter(), RTL_TEXTENCODING_ASCII_US );
                aTmpPath += aPath;
                setenv( "PATH", aTmpPath.GetBuffer(), TRUE );
            }
            // Assign to STAR_RESOURCEPATH environment variable
            if ( aCmdPath.Len() )
            {
                aTmpPath = aCmdPath;
                if ( aResPath.Len() )
                    aTmpPath += ByteString( DirEntry::GetSearchDelimiter(), RTL_TEXTENCODING_ASCII_US );
                aTmpPath += aResPath;
                setenv( "STAR_RESOURCEPATH", aTmpPath.GetBuffer(), TRUE );
            }
            // Assign to DYLD_LIBRARY_PATH environment variable
            if ( aCmdPath.Len() )
            {
                aTmpPath = aCmdPath;
                if ( aLibPath.Len() )
                    aTmpPath += ByteString( DirEntry::GetSearchDelimiter(), RTL_TEXTENCODING_ASCII_US );
                aTmpPath += aLibPath;
                setenv( "DYLD_LIBRARY_PATH", aTmpPath.GetBuffer(), TRUE );
            }
        }
    }
}

// -----------------------------------------------------------------------

void DeInitSalMain()
{
}

// =======================================================================

SalYieldMutex::SalYieldMutex()
{
	mnCount	 = 0;
	mnThreadId  = 0;
}

void SalYieldMutex::acquire()
{
	OMutex::acquire();
	mnThreadId = vos::OThread::getCurrentIdentifier();
	mnCount++;
}

void SalYieldMutex::release()
{
	if ( mnThreadId == vos::OThread::getCurrentIdentifier() )
	{
		if ( mnCount == 1 )
			mnThreadId = 0;
		mnCount--;
	}
	OMutex::release();
}

sal_Bool SalYieldMutex::tryToAcquire()
{
	if ( OMutex::tryToAcquire() )
	{
		mnThreadId = vos::OThread::getCurrentIdentifier();
		mnCount++;
		return sal_True;
	}
	else
		return sal_False;
}

// -----------------------------------------------------------------------

// some convenience functions regarding the yield mutex, aka solar mutex

sal_Bool ImplSalYieldMutexTryToAcquire()
{
	AquaSalInstance* pInst = (AquaSalInstance*) GetSalData()->mpFirstInstance;
	if ( pInst )
		return pInst->mpSalYieldMutex->tryToAcquire();
	else
		return FALSE;
}

void ImplSalYieldMutexAcquire()
{
	AquaSalInstance* pInst = (AquaSalInstance*) GetSalData()->mpFirstInstance;
	if ( pInst )
		pInst->mpSalYieldMutex->acquire();
}

void ImplSalYieldMutexRelease()
{
	AquaSalInstance* pInst = (AquaSalInstance*) GetSalData()->mpFirstInstance;
	if ( pInst )
		pInst->mpSalYieldMutex->release();
}

// =======================================================================

SalInstance* CreateSalInstance()
{
    // this is the case for not using SVMain
    // not so good
    if( bNoSVMain )
        initNSApp();

    SalData* pSalData = GetSalData();
    DBG_ASSERT( pSalData->mpFirstInstance == NULL, "more than one instance created" );
    AquaSalInstance* pInst = new AquaSalInstance;
    
    // init instance (only one instance in this version !!!)
    pSalData->mpFirstInstance = pInst;
    // this one is for outside AquaSalInstance::Yield
    SalData::ensureThreadAutoreleasePool();
    // no focus rects on NWF aqua
    ImplGetSVData()->maNWFData.mbNoFocusRects = true;
	ImplGetSVData()->maNWFData.mbNoBoldTabFocus = true;
    ImplGetSVData()->maNWFData.mbNoActiveTabTextRaise = true;
	ImplGetSVData()->maNWFData.mbCenteredTabs = true;
	ImplGetSVData()->maNWFData.mbProgressNeedsErase = true;
	ImplGetSVData()->maNWFData.mbCheckBoxNeedsErase = true;
	ImplGetSVData()->maNWFData.mnStatusBarLowerRightOffset = 10;
	ImplGetSVData()->maGDIData.mbNoXORClipping = true;
	ImplGetSVData()->maWinData.mbNoSaveBackground = true;
    
    return pInst;
}

// -----------------------------------------------------------------------

void DestroySalInstance( SalInstance* pInst )
{
	delete pInst;
}

// -----------------------------------------------------------------------

AquaSalInstance::AquaSalInstance()
{
	mpSalYieldMutex = new SalYieldMutex;
	mpSalYieldMutex->acquire();
	::tools::SolarMutex::SetSolarMutex( mpSalYieldMutex );
    maMainThread = vos::OThread::getCurrentIdentifier();
    mbWaitingYield = false;
    maUserEventListMutex = osl_createMutex();
    mnActivePrintJobs = 0;
    maWaitingYieldCond = osl_createCondition();
}

// -----------------------------------------------------------------------

AquaSalInstance::~AquaSalInstance()
{
	::tools::SolarMutex::SetSolarMutex( 0 );
	mpSalYieldMutex->release();
	delete mpSalYieldMutex;
    osl_destroyMutex( maUserEventListMutex );
    osl_destroyCondition( maWaitingYieldCond );
}

// -----------------------------------------------------------------------

void AquaSalInstance::wakeupYield()
{
    // wakeup :Yield
    if( mbWaitingYield )
    {
        SalData::ensureThreadAutoreleasePool();
        NSEvent* pEvent = [NSEvent otherEventWithType: NSApplicationDefined
                                   location: NSZeroPoint
                                   modifierFlags: 0
                                   timestamp: 0
                                   windowNumber: 0
                                   context: nil
                                   subtype: AquaSalInstance::YieldWakeupEvent
                                   data1: 0
                                   data2: 0 ];
        if( pEvent )
            [NSApp postEvent: pEvent atStart: NO];
    }
}

// -----------------------------------------------------------------------

void AquaSalInstance::PostUserEvent( AquaSalFrame* pFrame, sal_uInt16 nType, void* pData )
{
    osl_acquireMutex( maUserEventListMutex );
    maUserEvents.push_back( SalUserEvent( pFrame, pData, nType ) );
    osl_releaseMutex( maUserEventListMutex );
    
    // notify main loop that an event has arrived
    wakeupYield();
}

// -----------------------------------------------------------------------

vos::IMutex* AquaSalInstance::GetYieldMutex()
{
	return mpSalYieldMutex;
}

// -----------------------------------------------------------------------

sal_uLong AquaSalInstance::ReleaseYieldMutex()
{
	SalYieldMutex* pYieldMutex = mpSalYieldMutex;
	if ( pYieldMutex->GetThreadId() ==
		 vos::OThread::getCurrentIdentifier() )
	{
		sal_uLong nCount = pYieldMutex->GetAcquireCount();
		sal_uLong n = nCount;
		while ( n )
		{
			pYieldMutex->release();
			n--;
		}

		return nCount;
	}
	else
		return 0;
}

// -----------------------------------------------------------------------

void AquaSalInstance::AcquireYieldMutex( sal_uLong nCount )
{
	SalYieldMutex* pYieldMutex = mpSalYieldMutex;
	while ( nCount )
	{
		pYieldMutex->acquire();
		nCount--;
	}
}

// -----------------------------------------------------------------------

bool AquaSalInstance::CheckYieldMutex()
{
    bool bRet = true;

	SalYieldMutex* pYieldMutex = mpSalYieldMutex;
	if ( pYieldMutex->GetThreadId() !=
		 vos::OThread::getCurrentIdentifier() )
	{
	    bRet = false;
	}
    
    return bRet;
}

// -----------------------------------------------------------------------

bool AquaSalInstance::isNSAppThread() const
{
    return vos::OThread::getCurrentIdentifier() == maMainThread;
}

// -----------------------------------------------------------------------

void AquaSalInstance::handleAppDefinedEvent( NSEvent* pEvent )
{
    switch( [pEvent subtype] )
    {
    case AppStartTimerEvent:
        AquaSalTimer::handleStartTimerEvent( pEvent );
        break;
    case AppEndLoopEvent:
        [NSApp stop: NSApp];
        break;
    case AppExecuteSVMain:
    {
        sal_Bool bResult = ImplSVMain();
        if( gpbInit )
            *gpbInit = bResult;
        [NSApp stop: NSApp];
        bLeftMain = true;
        if( pDockMenu )
        {
            [pDockMenu release];
            pDockMenu = nil;
        }
    }
    break;
    case AppleRemoteEvent:
    {
        sal_Int16 nCommand = 0;
        SalData* pSalData = GetSalData();
        bool bIsFullScreenMode = false;

        std::list<AquaSalFrame*>::iterator it = pSalData->maFrames.begin();
        for(; it != pSalData->maFrames.end(); ++it )
        {
            if( (*it)->mbFullScreen )
                bIsFullScreenMode = true;
        }

        switch ([pEvent data1]) 
        {
            case kRemoteButtonPlay:
                nCommand = ( bIsFullScreenMode == true ) ? MEDIA_COMMAND_PLAY_PAUSE : MEDIA_COMMAND_PLAY;
                break;

            // kept for experimentation purpose (scheduled for future implementation)
            // case kRemoteButtonMenu:         nCommand = MEDIA_COMMAND_MENU; break;

            case kRemoteButtonPlus:     	nCommand = MEDIA_COMMAND_VOLUME_UP; break;
            
            case kRemoteButtonMinus:        nCommand = MEDIA_COMMAND_VOLUME_DOWN; break;

            case kRemoteButtonRight:        nCommand = MEDIA_COMMAND_NEXTTRACK; break;

            case kRemoteButtonRight_Hold:   nCommand = MEDIA_COMMAND_NEXTTRACK_HOLD; break;

            case kRemoteButtonLeft:         nCommand = MEDIA_COMMAND_PREVIOUSTRACK; break;

            case kRemoteButtonLeft_Hold:    nCommand = MEDIA_COMMAND_REWIND; break;

            case kRemoteButtonPlay_Hold:    nCommand = MEDIA_COMMAND_PLAY_HOLD; break;

            case kRemoteButtonMenu_Hold:    nCommand = MEDIA_COMMAND_STOP; break;

            // FIXME : not detected
            case kRemoteButtonPlus_Hold:
            case kRemoteButtonMinus_Hold:
                break;

            default:
                break;
        }
        AquaSalFrame* pFrame = pSalData->maFrames.front();
        Window* pWindow = pFrame ? pFrame->GetWindow() : NULL;

        if( pWindow )
        {
            const Point aPoint;
            CommandEvent aCEvt( aPoint, COMMAND_MEDIA, FALSE, &nCommand );
            NotifyEvent aNCmdEvt( EVENT_COMMAND, pWindow, &aCEvt );

            if ( !ImplCallPreNotify( aNCmdEvt ) )
                pWindow->Command( aCEvt );
        }

    }
	break;

    case YieldWakeupEvent:
        // do nothing, fall out of Yield
	break;

    default:
        DBG_ERROR( "unhandled NSApplicationDefined event" );
        break;
    };
}

// -----------------------------------------------------------------------

class ReleasePoolHolder
{
    NSAutoreleasePool* mpPool;
    public:
    ReleasePoolHolder() : mpPool( [[NSAutoreleasePool alloc] init] ) {}
    ~ReleasePoolHolder() { [mpPool release]; }
};

void AquaSalInstance::Yield( bool bWait, bool bHandleAllCurrentEvents )
{
    // ensure that the per thread autorelease pool is top level and
    // will therefore not be destroyed by cocoa implicitly
    SalData::ensureThreadAutoreleasePool();
    
    // NSAutoreleasePool documentation suggests we should have
    // an own pool for each yield level
    ReleasePoolHolder aReleasePool;
    
	// Release all locks so that we don't deadlock when we pull pending
	// events from the event queue
    bool bDispatchUser = true;
    while( bDispatchUser )
    {
        sal_uLong nCount = ReleaseYieldMutex();
    
        // get one user event
        osl_acquireMutex( maUserEventListMutex );
        SalUserEvent aEvent( NULL, NULL, 0 );
        if( ! maUserEvents.empty() )
        {
            aEvent = maUserEvents.front();
            maUserEvents.pop_front();
        }
        else
            bDispatchUser = false;
        osl_releaseMutex( maUserEventListMutex );
        
        AcquireYieldMutex( nCount );
        
        // dispatch it
        if( aEvent.mpFrame && AquaSalFrame::isAlive( aEvent.mpFrame ) )
        {
            aEvent.mpFrame->CallCallback( aEvent.mnType, aEvent.mpData );
            osl_setCondition( maWaitingYieldCond );
            // return if only one event is asked for
            if( ! bHandleAllCurrentEvents )
                return;
        }
    }
    
    // handle cocoa event queue
    // cocoa events mye be only handled in the thread the NSApp was created
    if( isNSAppThread() && mnActivePrintJobs == 0 )
    {
        // we need to be woken up by a cocoa-event
        // if a user event should be posted by the event handling below
        bool bOldWaitingYield = mbWaitingYield;
        mbWaitingYield = bWait;

        // handle available events
        NSEvent* pEvent = nil;
        bool bHadEvent = false;
        do
        {
            sal_uLong nCount = ReleaseYieldMutex();
    
            pEvent = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: nil
                            inMode: NSDefaultRunLoopMode dequeue: YES];
            if( pEvent )
            {
                [NSApp sendEvent: pEvent];
                bHadEvent = true;
            }
            [NSApp updateWindows];
        
            AcquireYieldMutex( nCount );
        } while( bHandleAllCurrentEvents && pEvent );
        
        // if we had no event yet, wait for one if requested
        if( bWait && ! bHadEvent )
        {
            sal_uLong nCount = ReleaseYieldMutex();
    
            NSDate* pDt = AquaSalTimer::pRunningTimer ? [AquaSalTimer::pRunningTimer fireDate] : [NSDate distantFuture];
            pEvent = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: pDt
                            inMode: NSDefaultRunLoopMode dequeue: YES];
            if( pEvent )
                [NSApp sendEvent: pEvent];
            [NSApp updateWindows];
        
            AcquireYieldMutex( nCount );

            // #i86581#
            // FIXME: sometimes the NSTimer will never fire. Firing it by hand then
            // fixes the problem even seems to set the correct next firing date
            // Why oh why ?
            if( ! pEvent && AquaSalTimer::pRunningTimer )
            {
                // this cause crashes on MacOSX 10.4
                // [AquaSalTimer::pRunningTimer fire];
                ImplGetSVData()->mpSalTimer->CallCallback();
            }
        }

        mbWaitingYield = bOldWaitingYield;
        
        // collect update rectangles
        const std::list< AquaSalFrame* > rFrames( GetSalData()->maFrames );
        for( std::list< AquaSalFrame* >::const_iterator it = rFrames.begin(); it != rFrames.end(); ++it )
        {
            if( (*it)->mbShown && ! (*it)->maInvalidRect.IsEmpty() )
            {
                (*it)->Flush( (*it)->maInvalidRect );
                (*it)->maInvalidRect.SetEmpty();
            }
        }
        osl_setCondition( maWaitingYieldCond );
    }
    else if( bWait )
    {
        // #i103162#
        // wait until any thread (most likely the main thread)
        // has dispatched an event, cop out at 500 ms
        sal_uLong nCount;
        TimeValue aVal = { 0, 500000000 };
        osl_resetCondition( maWaitingYieldCond );
        nCount = ReleaseYieldMutex();
        osl_waitCondition( maWaitingYieldCond, &aVal );
        AcquireYieldMutex( nCount );
    }
	// we get some apple events way too early
	// before the application is ready to handle them,
	// so their corresponding application events need to be delayed
	// now is a good time to handle at least one of them
	if( bWait && !aAppEventList.empty() && ImplGetSVData()->maAppData.mbInAppExecute )
	{
		// make sure that only one application event is active at a time
		static bool bInAppEvent = false;
		if( !bInAppEvent )
		{
			bInAppEvent = true;
			// get the next delayed application event
			const ApplicationEvent* pAppEvent = aAppEventList.front();
			aAppEventList.pop_front();
			// handle one application event (no recursion)
			const ImplSVData* pSVData = ImplGetSVData();
			pSVData->mpApp->AppEvent( *pAppEvent );
			delete pAppEvent;
			// allow the next delayed application event
			bInAppEvent = false;
		}
	}
}

// -----------------------------------------------------------------------

bool AquaSalInstance::AnyInput( sal_uInt16 nType )
{
    if( nType & INPUT_APPEVENT )
    {
        if( ! aAppEventList.empty() )
            return true;
        if( nType == INPUT_APPEVENT )
            return false;
    }
    
    if( nType & INPUT_TIMER )
    {
        if( AquaSalTimer::pRunningTimer )
        {
            NSDate* pDt = [AquaSalTimer::pRunningTimer fireDate];
            if( pDt && [pDt timeIntervalSinceNow] < 0 )
            {
                return true;
            }
        }
    }
        
	unsigned/*NSUInteger*/ nEventMask = 0;
	if( nType & INPUT_MOUSE)
		nEventMask |=
			NSLeftMouseDownMask    | NSRightMouseDownMask    | NSOtherMouseDownMask |
			NSLeftMouseUpMask      | NSRightMouseUpMask      | NSOtherMouseUpMask |
			NSLeftMouseDraggedMask | NSRightMouseDraggedMask | NSOtherMouseDraggedMask |
			NSScrollWheelMask |
			// NSMouseMovedMask |
			NSMouseEnteredMask | NSMouseExitedMask;
	if( nType & INPUT_KEYBOARD)
		nEventMask |= NSKeyDownMask | NSKeyUpMask | NSFlagsChangedMask;
	if( nType & INPUT_OTHER)
		nEventMask |= NSTabletPoint;
	// TODO: INPUT_PAINT / more INPUT_OTHER
	if( !nType)
		return false;

        NSEvent* pEvent = [NSApp nextEventMatchingMask: nEventMask untilDate: nil
                            inMode: NSDefaultRunLoopMode dequeue: NO];
	return (pEvent != NULL);
}

// -----------------------------------------------------------------------

SalFrame* AquaSalInstance::CreateChildFrame( SystemParentData*, sal_uLong /*nSalFrameStyle*/ )
{
	return NULL;
}

// -----------------------------------------------------------------------

SalFrame* AquaSalInstance::CreateFrame( SalFrame* pParent, sal_uLong nSalFrameStyle )
{
    SalData::ensureThreadAutoreleasePool();
    
    SalFrame* pFrame = new AquaSalFrame( pParent, nSalFrameStyle );
    return pFrame;
}

// -----------------------------------------------------------------------

void AquaSalInstance::DestroyFrame( SalFrame* pFrame )
{
	delete pFrame;
}

// -----------------------------------------------------------------------

SalObject* AquaSalInstance::CreateObject( SalFrame* pParent, SystemWindowData* /* pWindowData */, sal_Bool /* bShow */ )
{
    // SystemWindowData is meaningless on Mac OS X
	AquaSalObject *pObject = NULL;

	if ( pParent )
		pObject = new AquaSalObject( static_cast<AquaSalFrame*>(pParent) );

	return pObject;
}

// -----------------------------------------------------------------------

void AquaSalInstance::DestroyObject( SalObject* pObject )
{
	delete ( pObject );
}

// -----------------------------------------------------------------------

SalPrinter* AquaSalInstance::CreatePrinter( SalInfoPrinter* pInfoPrinter )
{
	return new AquaSalPrinter( dynamic_cast<AquaSalInfoPrinter*>(pInfoPrinter) );
}

// -----------------------------------------------------------------------

void AquaSalInstance::DestroyPrinter( SalPrinter* pPrinter )
{
    delete pPrinter;
}

// -----------------------------------------------------------------------

void AquaSalInstance::GetPrinterQueueInfo( ImplPrnQueueList* pList )
{
    NSArray* pNames = [NSPrinter printerNames];
    NSArray* pTypes = [NSPrinter printerTypes];
    unsigned int nNameCount = pNames ? [pNames count] : 0;
    unsigned int nTypeCount = pTypes ? [pTypes count] : 0;
    DBG_ASSERT( nTypeCount == nNameCount, "type count not equal to printer count" );
    for( unsigned int i = 0; i < nNameCount; i++ )
    {
        NSString* pName = [pNames objectAtIndex: i];
        NSString* pType = i < nTypeCount ? [pTypes objectAtIndex: i] : nil;
        if( pName )
        {
            SalPrinterQueueInfo* pInfo = new SalPrinterQueueInfo;
            pInfo->maPrinterName    = GetOUString( pName );
            if( pType )
                pInfo->maDriver     = GetOUString( pType );
            pInfo->mnStatus         = 0;
            pInfo->mnJobs           = 0;
            pInfo->mpSysData        = NULL;
            
            pList->Add( pInfo );
        }
    }
}

// -----------------------------------------------------------------------

void AquaSalInstance::GetPrinterQueueState( SalPrinterQueueInfo* )
{
}

// -----------------------------------------------------------------------

void AquaSalInstance::DeletePrinterQueueInfo( SalPrinterQueueInfo* pInfo )
{
    delete pInfo;
}

// -----------------------------------------------------------------------

XubString AquaSalInstance::GetDefaultPrinter()
{
    // #i113170# may not be the main thread if called from UNO API
    SalData::ensureThreadAutoreleasePool();
    
	if( ! maDefaultPrinter.getLength() )
    {
        NSPrintInfo* pPI = [NSPrintInfo sharedPrintInfo];
        DBG_ASSERT( pPI, "no print info" );
        if( pPI )
        {
            NSPrinter* pPr = [pPI printer];
            DBG_ASSERT( pPr, "no printer in default info" );
            if( pPr )
            {
                NSString* pDefName = [pPr name];
                DBG_ASSERT( pDefName, "printer has no name" );
                maDefaultPrinter = GetOUString( pDefName );
            }
        }
    }
    return maDefaultPrinter;
}

// -----------------------------------------------------------------------

SalInfoPrinter* AquaSalInstance::CreateInfoPrinter( SalPrinterQueueInfo* pQueueInfo,
												ImplJobSetup* pSetupData )
{
    // #i113170# may not be the main thread if called from UNO API
    SalData::ensureThreadAutoreleasePool();
    
	SalInfoPrinter* pNewInfoPrinter = NULL;
    if( pQueueInfo )
    {
        pNewInfoPrinter = new AquaSalInfoPrinter( *pQueueInfo );
        if( pSetupData )
            pNewInfoPrinter->SetPrinterData( pSetupData );
    }

    return pNewInfoPrinter;
}

// -----------------------------------------------------------------------

void AquaSalInstance::DestroyInfoPrinter( SalInfoPrinter* pPrinter )
{
    // #i113170# may not be the main thread if called from UNO API
    SalData::ensureThreadAutoreleasePool();
    
    delete pPrinter;
}

// -----------------------------------------------------------------------

SalSystem* AquaSalInstance::CreateSystem()
{
	return new AquaSalSystem();
}

// -----------------------------------------------------------------------

void AquaSalInstance::DestroySystem( SalSystem* pSystem )
{
	delete pSystem;
}

// -----------------------------------------------------------------------

void AquaSalInstance::SetEventCallback( void*, bool(*)(void*,void*,int) )
{
}

// -----------------------------------------------------------------------

void AquaSalInstance::SetErrorEventCallback( void*, bool(*)(void*,void*,int) )
{
}

// -----------------------------------------------------------------------

void* AquaSalInstance::GetConnectionIdentifier( ConnectionIdentifierType& rReturnedType, int& rReturnedBytes )
{
	rReturnedBytes	= 1;
	rReturnedType	= AsciiCString;
	return (void*)"";
}

// We need to re-encode file urls because osl_getFileURLFromSystemPath converts
// to UTF-8 before encoding non ascii characters, which is not what other apps expect.
static rtl::OUString translateToExternalUrl(const rtl::OUString& internalUrl)
{
    rtl::OUString extUrl;
        
    uno::Reference< lang::XMultiServiceFactory > sm = comphelper::getProcessServiceFactory();
    if (sm.is())
    {
        uno::Reference< beans::XPropertySet > pset;
        sm->queryInterface( getCppuType( &pset )) >>= pset;
        if (pset.is())
        {
            uno::Reference< uno::XComponentContext > context;
            static const rtl::OUString DEFAULT_CONTEXT( RTL_CONSTASCII_USTRINGPARAM( "DefaultContext" ) );
            pset->getPropertyValue(DEFAULT_CONTEXT) >>= context;
            if (context.is())
                extUrl = uri::ExternalUriReferenceTranslator::create(context)->translateToExternal(internalUrl);
        }
    }
    return extUrl;
}

// #i104525# many versions of OSX have problems with some URLs:
// when an app requests OSX to add one of these URLs to the "Recent Items" list
// then this app gets killed (TextEdit, Preview, etc. and also OOo)
static bool isDangerousUrl( const rtl::OUString& rUrl )
{
	// use a heuristic that detects all known cases since there is no official comment
	// on the exact impact and root cause of the OSX bug
	const int nLen = rUrl.getLength();
	const sal_Unicode* p = rUrl.getStr();
	for( int i = 0; i < nLen-3; ++i, ++p ) {
		if( p[0] != '%' )
			continue;
		// escaped percent?
		if( (p[1] == '2') && (p[2] == '5') )
			return true;
		// escapes are considered to be UTF-8 encoded
		// => check for invalid UTF-8 leading byte
		if( (p[1] != 'f') && (p[1] != 'F') )
			continue;
		int cLowNibble = p[2];
		if( (cLowNibble >= '0' ) && (cLowNibble <= '9'))
			return false;
		if( cLowNibble >= 'a' )
			cLowNibble -= 'a' - 'A';
		if( (cLowNibble < 'A') || (cLowNibble >= 'C'))
			return true;
	}

	return false;
}

void AquaSalInstance::AddToRecentDocumentList(const rtl::OUString& rFileUrl, const rtl::OUString& /*rMimeType*/)
{
    // Convert file URL for external use (see above)
    rtl::OUString externalUrl = translateToExternalUrl(rFileUrl);
    if( 0 == externalUrl.getLength() )
        externalUrl = rFileUrl;
    
    if( externalUrl.getLength() && !isDangerousUrl( externalUrl ) )
    {
        NSString* pString = CreateNSString( externalUrl );
        NSURL* pURL = [NSURL URLWithString: pString];

        if( pURL )
        {
            NSDocumentController* pCtrl = [NSDocumentController sharedDocumentController];
            [pCtrl noteNewRecentDocumentURL: pURL];
        }
        if( pString )
            [pString release];
    }
}


// -----------------------------------------------------------------------

SalTimer* AquaSalInstance::CreateSalTimer()
{
    return new AquaSalTimer();
}

// -----------------------------------------------------------------------

SalSystem* AquaSalInstance::CreateSalSystem()
{
    return new AquaSalSystem();
}

// -----------------------------------------------------------------------

SalBitmap* AquaSalInstance::CreateSalBitmap()
{
    return new AquaSalBitmap();
}

// -----------------------------------------------------------------------

SalSession* AquaSalInstance::CreateSalSession()
{
    return NULL;
}

// -----------------------------------------------------------------------

class MacImeStatus : public SalI18NImeStatus
{
public:
    MacImeStatus() {}
    virtual ~MacImeStatus() {}

    // asks whether there is a status window available
    // to toggle into menubar
    virtual bool canToggle() { return false; }
    virtual void toggle() {}
};

// -----------------------------------------------------------------------

SalI18NImeStatus* AquaSalInstance::CreateI18NImeStatus()
{
    return new MacImeStatus();
}

// YieldMutexReleaser
YieldMutexReleaser::YieldMutexReleaser() : mnCount( 0 )
{
    SalData* pSalData = GetSalData();
    if( ! pSalData->mpFirstInstance->isNSAppThread() )
    {
        SalData::ensureThreadAutoreleasePool();
        mnCount = pSalData->mpFirstInstance->ReleaseYieldMutex();
    }
}

YieldMutexReleaser::~YieldMutexReleaser()
{
    if( mnCount != 0 )
        GetSalData()->mpFirstInstance->AcquireYieldMutex( mnCount );
}

//////////////////////////////////////////////////////////////
rtl::OUString GetOUString( CFStringRef rStr )
{
    if( rStr == 0 )
        return rtl::OUString();
    CFIndex nLength = CFStringGetLength( rStr );
    if( nLength == 0 )
        return rtl::OUString();
    const UniChar* pConstStr = CFStringGetCharactersPtr( rStr );
    if( pConstStr )
        return rtl::OUString( pConstStr, nLength );
    UniChar* pStr = reinterpret_cast<UniChar*>( rtl_allocateMemory( sizeof(UniChar)*nLength ) );
    CFRange aRange = { 0, nLength };
    CFStringGetCharacters( rStr, aRange, pStr );
    rtl::OUString aRet( pStr, nLength );
    rtl_freeMemory( pStr );
    return aRet;
}

rtl::OUString GetOUString( NSString* pStr )
{
    if( ! pStr )
        return rtl::OUString();
    int nLen = [pStr length];
    if( nLen == 0 )
        return rtl::OUString();
    
    rtl::OUStringBuffer aBuf( nLen+1 );
    aBuf.setLength( nLen );
    [pStr getCharacters: const_cast<sal_Unicode*>(aBuf.getStr())];
    return aBuf.makeStringAndClear();
}

CFStringRef CreateCFString( const rtl::OUString& rStr )
{
    return CFStringCreateWithCharacters(kCFAllocatorDefault, rStr.getStr(), rStr.getLength() );
}

NSString* CreateNSString( const rtl::OUString& rStr )
{
    return [[NSString alloc] initWithCharacters: rStr.getStr() length: rStr.getLength()];
}

CGImageRef CreateCGImage( const Image& rImage )
{
    BitmapEx aBmpEx( rImage.GetBitmapEx() );
    Bitmap aBmp( aBmpEx.GetBitmap() );
        
    if( ! aBmp || ! aBmp.ImplGetImpBitmap() )
        return NULL;
    
    // simple case, no transparency
    AquaSalBitmap* pSalBmp = static_cast<AquaSalBitmap*>(aBmp.ImplGetImpBitmap()->ImplGetSalBitmap());
    
    if( ! pSalBmp )
        return NULL;
    
    CGImageRef xImage = NULL;
    if( ! (aBmpEx.IsAlpha() || aBmpEx.IsTransparent() ) )
        xImage = pSalBmp->CreateCroppedImage( 0, 0, pSalBmp->mnWidth, pSalBmp->mnHeight );
    else if( aBmpEx.IsAlpha() )
    {
        AlphaMask aAlphaMask( aBmpEx.GetAlpha() );
        Bitmap aMask( aAlphaMask.GetBitmap() );
        AquaSalBitmap* pMaskBmp = static_cast<AquaSalBitmap*>(aMask.ImplGetImpBitmap()->ImplGetSalBitmap());
        if( pMaskBmp )
            xImage = pSalBmp->CreateWithMask( *pMaskBmp, 0, 0, pSalBmp->mnWidth, pSalBmp->mnHeight );
        else
            xImage = pSalBmp->CreateCroppedImage( 0, 0, pSalBmp->mnWidth, pSalBmp->mnHeight );
    }
    else if( aBmpEx.GetTransparentType() == TRANSPARENT_BITMAP )
    {
        Bitmap aMask( aBmpEx.GetMask() );
        AquaSalBitmap* pMaskBmp = static_cast<AquaSalBitmap*>(aMask.ImplGetImpBitmap()->ImplGetSalBitmap());
        if( pMaskBmp )
            xImage = pSalBmp->CreateWithMask( *pMaskBmp, 0, 0, pSalBmp->mnWidth, pSalBmp->mnHeight );
        else
            xImage = pSalBmp->CreateCroppedImage( 0, 0, pSalBmp->mnWidth, pSalBmp->mnHeight );
    }
    else if( aBmpEx.GetTransparentType() == TRANSPARENT_COLOR )
    {
        Color aTransColor( aBmpEx.GetTransparentColor() );
        SalColor nTransColor = MAKE_SALCOLOR( aTransColor.GetRed(), aTransColor.GetGreen(), aTransColor.GetBlue() );
        xImage = pSalBmp->CreateColorMask( 0, 0, pSalBmp->mnWidth, pSalBmp->mnHeight, nTransColor );
    }
    
    return xImage;
}

NSImage* CreateNSImage( const Image& rImage )
{
    CGImageRef xImage = CreateCGImage( rImage );
    
    if( ! xImage )
        return nil;
    
    Size aSize( rImage.GetSizePixel() );
    NSImage* pImage = [[NSImage alloc] initWithSize: NSMakeSize( aSize.Width(), aSize.Height() )];
    if( pImage )
    {
        [pImage setFlipped: YES];
        [pImage lockFocus];
        
        NSGraphicsContext* pContext = [NSGraphicsContext currentContext];
        CGContextRef rCGContext = reinterpret_cast<CGContextRef>([pContext graphicsPort]);
        
        const CGRect aDstRect = CGRectMake( 0, 0, aSize.Width(), aSize.Height());
        CGContextDrawImage( rCGContext, aDstRect, xImage );
        
        [pImage unlockFocus];
    }

    CGImageRelease( xImage );

    return pImage;
}
