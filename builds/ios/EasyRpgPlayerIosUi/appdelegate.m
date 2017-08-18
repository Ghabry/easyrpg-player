//
//  appdelegate.m
//  SDLTest
//
//  Created by EasyRPG on 16.08.17.
//  Copyright © 2017 EasyRPG. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "appdelegate.h"

@implementation EasyRpgAppDelegate

-(id) init {
    self = [super init];
    return self;
}
    
// override the direct execution of SDL_main to allow us to implement our own frontend
-(void) postFinishLaunch
{

}
    
@end
