//
//  main.m
//  SDLTest
//
//  Created by EasyRPG on 16.08.17.
//  Copyright © 2017 EasyRPG. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "../SDL2/SDL_uikitappdelegate.h"

#import "EasyRpgPlayerIosUi-Swift.h"

#import "appdelegate.h"
#include <SDL.h>
#include <SDL_syswm.h>

#define SCREEN_TARGET_WIDTH 320
#define SCREEN_TARGET_HEIGHT 240

@implementation SDLUIKitDelegate (customDelegate)
+(NSString *)getAppDelegateClassName {
    return @"EasyRpgAppDelegate";
}
@end

UIViewController * GetSDLViewController(SDL_Window * sdlWindow)
{
    SDL_SysWMinfo systemWindowInfo;
    SDL_VERSION(&systemWindowInfo.version);
    if (!SDL_GetWindowWMInfo(sdlWindow, &systemWindowInfo)) {
        // consider doing some kind of error handling here
        return nil;
    }
    UIWindow * appWindow = systemWindowInfo.info.uikit.window;
    UIViewController * rootViewController = appWindow.rootViewController;
    return rootViewController;
}
@class EasyRpgPlayerHandler;

void SetupIosControls(SDL_Window * sdlWindow)
{
    UIViewController * rootViewController = GetSDLViewController(sdlWindow);
    if (!rootViewController ) {
        // handle this error as appropriate
    } else {
        EasyRpgPlayerViewController * view = [[EasyRpgPlayerViewController alloc] initWithNibName:@"EasyRpgPlayer" bundle:nil];
        [rootViewController.view addSubview:view.view];
        [view setupUi];
    }
}

#define USE_SDL
#define SUPPORT_AUDIO

extern "C" int main(int argc, char** argv) {
    // make the linker happy
    return 0;
}

// Work around namespace pollution problems
#define Rect Rect2

#include "baseui.h"
#include "main.h"
#include "main_data.h"
#include "player.h"
#include "sdl_ui.h"

int start_sdl_intern2() {
    Main_Data::SetProjectPath("./TestGame-2000");
        
    Player::Init(0, 0);
    
    SDL_Window* window = static_cast<SdlUi*>(DisplayUi.get())->GetSdlWindow();
    
    SetupIosControls(window);
    
    Player::Run();
        
    return 0;
}

extern "C" int start_sdl_intern() {
    return start_sdl_intern2();
}
