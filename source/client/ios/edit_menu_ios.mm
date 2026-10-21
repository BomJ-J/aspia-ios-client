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

#include "client/ios/edit_menu.h"

#include <QWidget>
#include <QWindow>

#import <UIKit/UIKit.h>

#include <vector>

// Supplies the menu and the rectangle it points at to the interaction. The actions are the ones of
// the last showEditMenu() call.
@interface AspiaEditMenuDelegate : NSObject <UIEditMenuInteractionDelegate>
{
@public
    std::vector<EditMenuItem> items;
    CGRect target;
}
@end

@implementation AspiaEditMenuDelegate

- (UIMenu*)editMenuInteraction:(UIEditMenuInteraction*)interaction
          menuForConfiguration:(UIEditMenuConfiguration*)configuration
              suggestedActions:(NSArray<UIMenuElement*>*)suggestedActions
{
    Q_UNUSED(interaction);
    Q_UNUSED(configuration);
    Q_UNUSED(suggestedActions);

    NSMutableArray<UIMenuElement*>* actions = [NSMutableArray array];
    for (const EditMenuItem& item : items)
    {
        std::function<void()> callback = item.action;
        [actions addObject:[UIAction actionWithTitle:item.title.toNSString()
                                               image:nil
                                          identifier:nil
                                             handler:^(UIAction* action)
        {
            Q_UNUSED(action);
            if (callback)
                callback();
        }]];
    }

    return [UIMenu menuWithChildren:actions];
}

- (CGRect)editMenuInteraction:(UIEditMenuInteraction*)interaction
   targetRectForConfiguration:(UIEditMenuConfiguration*)configuration
{
    Q_UNUSED(interaction);
    Q_UNUSED(configuration);
    return target;
}

@end

//--------------------------------------------------------------------------------------------------
void showEditMenu(QWidget* field, const QRect& target, const QList<EditMenuItem>& items)
{
    if (!field || items.isEmpty())
        return;

    QWidget* top_level = field->window();
    QWindow* window = top_level ? top_level->windowHandle() : nullptr;
    if (!window)
        return;

    UIView* view = reinterpret_cast<UIView*>(window->winId());
    if (!view)
        return;

    // One interaction for the whole run, moved to the view of the window the field is in. Neither
    // is released: the view does not retain the delegate of its interaction.
    static AspiaEditMenuDelegate* delegate = [[AspiaEditMenuDelegate alloc] init];
    static UIEditMenuInteraction* interaction =
        [[UIEditMenuInteraction alloc] initWithDelegate:delegate];

    if (interaction.view != view)
    {
        [interaction.view removeInteraction:interaction];
        [view addInteraction:interaction];
    }

    delegate->items.assign(items.cbegin(), items.cend());

    // The widgets of a window share its native view, so widget coordinates mapped to the top-level
    // widget are coordinates in that view.
    const QRect rect(field->mapTo(top_level, target.topLeft()), target.size());
    delegate->target = rect.toCGRect();

    UIEditMenuConfiguration* configuration =
        [UIEditMenuConfiguration configurationWithIdentifier:nil
                                                 sourcePoint:CGPointMake(rect.center().x(), rect.top())];
    [interaction presentEditMenuWithConfiguration:configuration];
}
