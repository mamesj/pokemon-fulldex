#include "../frontend/menu_drag.h"
#include <cassert>

int main()
{
    MenuDrag drag;
    int scroll = 30;
    assert(!drag.End());
    drag.Begin(200, scroll);
    assert(!drag.Move(204, 12, scroll));
    assert(scroll == 30);
    assert(drag.End()); // A small amount of finger movement is still one tap.
    assert(!drag.End());
    drag.Begin(200, scroll);
    assert(drag.Move(140, 12, scroll));
    assert(scroll == 90); // A swipe up reveals later rows.
    assert(drag.Move(220, 12, scroll));
    assert(scroll == 10);
    assert(drag.Move(200, 12, scroll));
    assert(!drag.End()); // Returning to the start must not activate a setting.
    drag.Begin(200, 90);
    assert(drag.Move(240, 12, scroll));
    assert(scroll == 50); // A swipe down reveals earlier rows.
    assert(!drag.End());
}
