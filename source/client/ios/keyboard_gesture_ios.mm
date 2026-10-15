//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ios/keyboard_gesture.h"

#include <QtGlobal>

#import <UIKit/UIKit.h>

// A gesture recognizer delegate that lets no touch through to the recognizer it is set on. The
// recognizer of Qt keeps its delegate when Qt moves it from one window to another.
@interface AspiaGestureBlocker : NSObject <UIGestureRecognizerDelegate>
@end

@implementation AspiaGestureBlocker

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)recognizer shouldReceiveTouch:(UITouch*)touch
{
    Q_UNUSED(recognizer);
    Q_UNUSED(touch);
    return NO;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)recognizer
{
    Q_UNUSED(recognizer);
    return NO;
}

@end

//--------------------------------------------------------------------------------------------------
void disableKeyboardHideGesture()
{
    // The delegate property does not retain; the blocker lives for the whole run.
    static AspiaGestureBlocker* blocker = [[AspiaGestureBlocker alloc] init];

    @autoreleasepool
    {
        NSMutableArray<UIWindow*>* windows = [NSMutableArray array];
        for (UIScene* scene in [UIApplication sharedApplication].connectedScenes)
        {
            if ([scene isKindOfClass:[UIWindowScene class]])
                [windows addObjectsFromArray:static_cast<UIWindowScene*>(scene).windows];
        }

        for (UIWindow* window in windows)
        {
            for (UIGestureRecognizer* recognizer in window.gestureRecognizers)
            {
                if ([NSStringFromClass([recognizer class]) hasSuffix:@"QIOSKeyboardListener"])
                    recognizer.delegate = blocker;
            }
        }
    }
}
