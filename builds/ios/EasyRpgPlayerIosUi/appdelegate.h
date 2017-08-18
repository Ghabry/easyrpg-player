//
//  appdelegate.h
//  SDLTest
//
//  Created by EasyRPG on 16.08.17.
//  Copyright © 2017 EasyRPG. All rights reserved.
//

#ifndef appdelegate_h
#define appdelegate_h

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "../SDL2/SDL_uikitappdelegate.h"

int start_sdl();

@interface EasyRpgAppDelegate : SDLUIKitDelegate {

}

@end

#endif /* appdelegate_h */
